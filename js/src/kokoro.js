import { KokoroTTS, TextSplitterStream } from "../kokoro.js/dist/kokoro.js";
import { createRequire } from "module";
import { readFile, stat, writeFile } from "fs/promises";
import { encodeWAV } from "./wav.js";

// Create a require function specific to this module's scope
const require = createRequire(import.meta.url);

// Use the created require to import the .node addon
// Adjust the path as needed, using __dirname for relative resolution
const alsaPlayer = require(`../build/Release/alsa_player.node`);

// TTS general purpose
const model_id = "onnx-community/Kokoro-82M-v1.0-ONNX";

function flattenFloat32Arrays(arrayOfFloat32Arrays) {
  // 1. Calculate the total length of all the input arrays
  let totalLength = 0;
  for (const arr of arrayOfFloat32Arrays) {
    if (arr instanceof Float32Array) {
      totalLength += arr.length;
    } else {
      console.warn("Skipping non-Float32Array item in the array:", arr);
    }
  }

  // 2. Create a new Float32Array with the total length
  const flattenedArray = new Float32Array(totalLength);

  // 3. Copy the data from each source array into the new array
  let offset = 0;
  for (const arr of arrayOfFloat32Arrays) {
    if (arr instanceof Float32Array) {
      flattenedArray.set(arr, offset); // Copy 'arr' into 'flattenedArray' starting at 'offset'
      offset += arr.length; // Move the offset for the next array
    }
  }

  return flattenedArray;
}

// KokoroWrapper
export class KokoroWrapper {
  /**
   *
   * @param {*} filename a utf-8 text file
   * @param {*} options include 3 options, output filename, stream, or play including lang for language
   * @returns stream if option stream is true otherwise undefined
   */
  static async ttsFromFile(filename, options) {
    if (!options || (!options.stream && !options.play && !options.filename)) {
      throw Error(`invalid options ${options}`);
    }

    const tts = await KokoroTTS.from_pretrained(model_id, {
      dtype: "fp32", // Options: "fp32", "fp16", "q8", "q4", "q4f16"
      device: "cpu", // Options: "wasm", "webgpu" (web) or "cpu" (node).
    });

    const stats = await stat(filename);
    if (!stats.isFile()) {
      throw Error(`${filename} is not a file`);
    }

    const voice = options?.voice ? options.voice : "af_heart";

    const splitter = new TextSplitterStream();
    const stream = tts.stream(splitter, {
      voice: voice, // english voice: af_heart,
    });

    // push text into stream
    (async () => {
      const content = await readFile(filename, "utf8");
      // capture a segement of text with complete paragraph
      const tokens = content.match(/\s*\S+/g);
      for (const token of tokens) {
        splitter.push(token);
        // delay 10 ms
        await new Promise((resolve) => setTimeout(resolve, 10));
      }
      splitter.close();
    })();

    if (options.stream === true) {
      // stream generator
      return async function* () {
        for await (const { text, phonemes, rawAudio } of stream) {
          yield rawAudio;
        }
      };
    }

    let audioFormat = null;
    let audioSampleRate = null;
    let audioBitDepth = null;
    let audioNumChannels = null;
    let audioHandler = null;
    let start = null;
    let finish = null;

    // save output to file
    if (options.filename) {
      let buff = [];
      start = async () => {};
      audioHandler = async (rawAudio) => {
        if (rawAudio && rawAudio.audio) {
          buff.push(rawAudio.audio);
          await new Promise((resolve) => setTimeout(resolve, 10));
        }
      };
      finish = async () => {
        const flatBuff = flattenFloat32Arrays(buff);
        let finalAudioBuffer = encodeWAV(
          flatBuff,
          audioSampleRate,
          audioNumChannels,
          audioBitDepth,
        );
        finalAudioBuffer = Buffer.from(finalAudioBuffer);
        await writeFile(options.filename, finalAudioBuffer);
      };
    }

    // play audio
    if (options.play) {
      let alsaPlayerStarted = false;
      start = async () => {
        alsaPlayer.startPlayback(
          {
            audioFormat: audioFormat, // 3 a floating 32 audio, if the audio is not float32 then value should be 1
            sampleRate: audioSampleRate,
            channels: audioNumChannels,
            bitDepth: audioBitDepth, // Passed directly to ALSA
          },
          (err) => {
            if (err) {
              throw err;
            }
          },
        );

        // handle kill and ctrl + c signal
        const terminate = () => {
          if (alsaPlayerStarted) {
            alsaPlayer.stopPlayback();
          }
          process.exit(0);
        };
        process.on("SIGINT", terminate);
        process.on("SIGTERM", terminate);
        alsaPlayerStarted = true;
      };
      audioHandler = async (rawAudio) => {
        if (rawAudio && rawAudio.audio && alsaPlayerStarted) {
          alsaPlayer.writeAudioData(rawAudio.audio);
          await new Promise((resolve) => setTimeout(resolve, 10));
        }
      };
      finish = async () => {
        if (alsaPlayerStarted) {
          alsaPlayer.stopPlayback();
        }
      };
    }

    const rawTTS = await stream.next();
    const rawAudio = rawTTS.value.audio;
    // audio a raw binary wave audio
    if (!rawAudio || !rawAudio.audio || !rawAudio.sampling_rate) {
      throw Error("Received invalid RawAudio chunk.", rawAudio);
    }

    audioSampleRate = rawAudio.sampling_rate;
    // Determine bit depth based on the TypedArray type
    if (rawAudio.audio instanceof Float32Array) {
      audioBitDepth = 32;
      audioFormat = 3;
    } else if (rawAudio.audio instanceof Int16Array) {
      audioBitDepth = 16;
      audioFormat = 1;
    } else {
      // Add other types if necessary (e.g., Int8Array, Uint8Array)
      throw Error(
        `Unknown audio sample type:, ${rawAudio.audio.constructor.name}`,
      );
    }
    // Assuming mono unless specified otherwise by kokoro-js/RawAudio
    audioNumChannels = 1; // You might need to adjust this if stereo

    await start();
    await audioHandler(rawAudio);

    for await (const { text, phonemes, audio } of stream) {
      if (!audio) {
        continue;
      }
      await audioHandler(audio);
    }
    await finish();
  }
}
