// alsa/alsa_player.cc
#include <napi.h>
#include <alsa/asoundlib.h> // Include ALSA headers
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

// Define a struct to hold audio format details (matching wav.Reader output)
struct AudioFormat {
    unsigned int audioFormat;
    unsigned int sampleRate;
    unsigned int channels;
    unsigned int bitDepth; // e.g., 16, 24, 32
    // Add other relevant format details if needed by ALSA
};

// --- ALSA Playback Worker (runs on a separate thread) ---
class AlsaPlaybackWorker : public Napi::AsyncWorker {
public:
    AlsaPlaybackWorker(Napi::Function& callback, AudioFormat format)
        : Napi::AsyncWorker(callback), _format(format), _pcm_handle(nullptr), _running(true) {
        // Initialize the speaker handle to null initially
    }

    ~AlsaPlaybackWorker() {
        if (_pcm_handle) {
            snd_pcm_drain(_pcm_handle);
            snd_pcm_close(_pcm_handle);
        }
    }

    void QueueAudioData(const Napi::Buffer<char>& data) {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _audioQueue.push(std::vector<char>(data.Data(), data.Data() + data.Length()));
        _queueCv.notify_one(); // Notify the worker thread that new data is available
    }

    void Stop() {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _running = false;
        _queueCv.notify_one(); // Wake up the worker thread
    }

protected:
    void Execute() override {
        // This runs on a separate thread
        int err;
        const char *pcm_device = "default"; // Or make configurable

        // Open PCM device
        if ((err = snd_pcm_open(&_pcm_handle, pcm_device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
            SetError("Can't open ALSA PCM device: " + std::string(snd_strerror(err)));
            return;
        }

        // Allocate hardware parameters object
        snd_pcm_hw_params_t *params;
        snd_pcm_hw_params_alloca(&params);
        snd_pcm_hw_params_any(_pcm_handle, params);

        // Set hardware parameters
        if ((err = snd_pcm_hw_params_set_access(_pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)) {
            SetError("Can't set interleaved mode: " + std::string(snd_strerror(err)));
            return;
        }

        snd_pcm_format_t pcm_format;
        // Map bitDepth to ALSA format
        // Determine the ALSA PCM format based on audioFormat and bitDepth
        if (_format.audioFormat == 3) { // This means it's IEEE Float
            if (_format.bitDepth == 32) {
                pcm_format = SND_PCM_FORMAT_FLOAT_LE; // 32-bit float (single precision)
            } else if (_format.bitDepth == 64) {
                pcm_format = SND_PCM_FORMAT_FLOAT64_LE; // 64-bit float (double precision)
            } else {
                SetError("Unsupported floating-point bit depth: " + std::to_string(_format.bitDepth));
                return;
            }
        } else if (_format.audioFormat == 1) { // This means it's PCM (integer)
            if (_format.bitDepth == 16) {
                pcm_format = SND_PCM_FORMAT_S16_LE;
            } else if (_format.bitDepth == 24) {
                // IMPORTANT: 24-bit can be tricky. Often it's 24-bit in 3 bytes (S24_3LE)
                // or 24-bit padded to 4 bytes (S24_LE). You need to know your source.
                // wav.Reader usually outputs S24_3LE if the file is 3-byte.
                // Assuming wav.Reader passes 24 for 24-bit.
                pcm_format = SND_PCM_FORMAT_S24_3LE; // Common for 24-bit in 3 bytes
                // OR: pcm_format = SND_PCM_FORMAT_S24_LE; // If 24-bit padded to 4 bytes
            } else if (_format.bitDepth == 32) {
                pcm_format = SND_PCM_FORMAT_S32_LE;
            } else {
                SetError("Unsupported integer bit depth: " + std::to_string(_format.bitDepth));
                return;
            }
        } else {
            SetError("Unsupported audio format tag: " + std::to_string(_format.audioFormat));
            return;
        }

        if ((err = snd_pcm_hw_params_set_format(_pcm_handle, params, pcm_format) < 0)) {
            SetError("Can't set format: " + std::string(snd_strerror(err)));
            return;
        }
        if ((err = snd_pcm_hw_params_set_channels(_pcm_handle, params, _format.channels) < 0)) {
            SetError("Can't set channels number: " + std::string(snd_strerror(err)));
            return;
        }
        unsigned int actualRate = _format.sampleRate; // ALSA might adjust this
        if ((err = snd_pcm_hw_params_set_rate_near(_pcm_handle, params, &actualRate, 0) < 0)) {
            SetError("Can't set rate: " + std::string(snd_strerror(err)));
            return;
        }
        if (actualRate != _format.sampleRate) {
            std::cerr << "Warning: ALSA adjusted sample rate from " << _format.sampleRate << " to " << actualRate << std::endl;
        }

        // Write parameters to PCM device
        if ((err = snd_pcm_hw_params(_pcm_handle, params) < 0)) {
            SetError("Can't set hardware parameters: " + std::string(snd_strerror(err)));
            return;
        }

        snd_pcm_uframes_t frames;
        snd_pcm_hw_params_get_period_size(params, &frames, 0);
        size_t bytes_per_frame = (_format.bitDepth / 8) * _format.channels;
        size_t period_size_bytes = frames * bytes_per_frame;

        // Main playback loop
        while (_running) {
            std::unique_lock<std::mutex> lock(_queueMutex);
            // Wait for data to be available or for the worker to be stopped
            _queueCv.wait(lock, [this]{ return !_audioQueue.empty() || !_running; });

            if (!_running && _audioQueue.empty()) {
                break; // Stop worker if no more data and not running
            }

            if (!_audioQueue.empty()) {
                std::vector<char> audioData = _audioQueue.front();
                _audioQueue.pop();
                lock.unlock(); // Release lock before writing to PCM

                size_t total_bytes_to_write = audioData.size();
                char* current_buffer_ptr = audioData.data();

                while (total_bytes_to_write > 0) {
                    snd_pcm_sframes_t frames_to_write = total_bytes_to_write / bytes_per_frame;
                    if (frames_to_write == 0) break; // Not enough bytes for a full frame

                    // Write audio data
                    err = snd_pcm_writei(_pcm_handle, current_buffer_ptr, frames_to_write);

                    if (err == -EPIPE) { // XRUN (underrun/overrun)
                        std::cerr << "ALSA XRUN, preparing PCM device." << std::endl;
                        snd_pcm_prepare(_pcm_handle);
                    } else if (err < 0) {
                        SetError("Error writing to ALSA PCM device: " + std::string(snd_strerror(err)));
                        return;
                    } else {
                        // Successfully wrote 'err' frames
                        size_t bytes_written = err * bytes_per_frame;
                        current_buffer_ptr += bytes_written;
                        total_bytes_to_write -= bytes_written;
                    }
                }
            }
        }
    }

    void OnOK() override {
        // This is called on the main thread when Execute completes successfully
        Napi::Env env = Env();
        Callback().Call({env.Undefined(), Napi::String::New(env, "Playback finished.")});
    }

    void OnError(const Napi::Error& e) override {
        // This is called on the main thread if SetError() was called in Execute()
        Napi::Env env = Env();
        Callback().Call({e.Value()});
    }

private:
    AudioFormat _format;
    snd_pcm_t *_pcm_handle;
    std::queue<std::vector<char>> _audioQueue;
    std::mutex _queueMutex;
    std::condition_variable _queueCv;
    bool _running;
};

// --- JavaScript Callable Functions ---

// Global pointer to the worker to allow sending data
AlsaPlaybackWorker* global_alsa_worker = nullptr;

// Declare as Napi::Reference, and use Reset() to assign
Napi::Reference<Napi::Function> global_js_callback; // Corrected declaration

// Function to start playback
Napi::Value StartPlayback(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsFunction()) {
        Napi::TypeError::New(env, "Expected format object and callback function").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Object formatObj = info[0].As<Napi::Object>();
    Napi::Function callback = info[1].As<Napi::Function>();

    AudioFormat format;
    format.audioFormat = formatObj.Get("audioFormat").As<Napi::Number>().Uint32Value();
    format.sampleRate = formatObj.Get("sampleRate").As<Napi::Number>().Uint32Value();
    format.channels = formatObj.Get("channels").As<Napi::Number>().Uint32Value();
    format.bitDepth = formatObj.Get("bitDepth").As<Napi::Number>().Uint32Value();

    // Basic validation for format
    if (format.channels < 1 || format.channels > 8 || format.sampleRate < 8000 || format.bitDepth % 8 != 0) {
         Napi::TypeError::New(env, "Invalid audio format parameters provided.").ThrowAsJavaScriptException();
         return env.Undefined();
    }


    if (global_alsa_worker) {
        Napi::Error::New(env, "Playback already started. Stop it first.").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    global_alsa_worker = new AlsaPlaybackWorker(callback, format);
    // Correct way to assign a new persistent reference
    global_js_callback.Reset(callback, 1); // Reset with the new callback and a ref count of 1
    // Alternatively, you could do: global_js_callback = Napi::Persistent(callback);
    // if global_js_callback was initially default-constructed and not an already
    // persistent reference. But Reset is generally safer for re-assignment.


    global_alsa_worker->Queue(); // Start the async worker thread

    return env.Undefined();
}

// Function to send audio data
Napi::Value WriteAudioData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsBuffer()) {
        Napi::TypeError::New(env, "Expected a Buffer as audio data").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (!global_alsa_worker) {
        Napi::Error::New(env, "Playback not started. Call startPlayback first.").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Buffer<char> audioData = info[0].As<Napi::Buffer<char>>();
    global_alsa_worker->QueueAudioData(audioData);

    return env.Undefined();
}

// In StopPlayback, ensure you reset the reference:
Napi::Value StopPlayback(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (global_alsa_worker) {
        global_alsa_worker->Stop();
        global_alsa_worker = nullptr; // Clear the global pointer
        global_js_callback.Reset(); // Release the persistent reference by resetting it
    } else {
        Napi::Error::New(env, "Playback not active.").ThrowAsJavaScriptException();
    }

    return env.Undefined();
}

// Module initialization
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "startPlayback"), Napi::Function::New(env, StartPlayback));
    exports.Set(Napi::String::New(env, "writeAudioData"), Napi::Function::New(env, WriteAudioData));
    exports.Set(Napi::String::New(env, "stopPlayback"), Napi::Function::New(env, StopPlayback));
    return exports;
}

NODE_API_MODULE(alsa_player, Init) // alsa_player is the module name
