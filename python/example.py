import argparse
import pyaudio
from kokoro import KPipeline
import soundfile as sf
import numpy as np
import os
from tkinter.constants import TOP

def main():
    parser = argparse.ArgumentParser(description="Text-to-speech with Kokoro, supporting multiple options.")

    # Positional argument for the input file
    parser.add_argument('input_file', type=str,
                        help='Path to the text file containing the input for text-to-speech.')

    # Optional argument for playing the audio (boolean flag)
    parser.add_argument('--play', action='store_true',
                        help='If set, the generated audio will be played after generation.')

    # Optional argument for specifying the voice
    parser.add_argument('--voice', type=str, default='af_heart',
                        help='Specify the voice to use for text-to-speech (e.g., af_heart). Default is af_heart.')

    # Optional argument for specifying the language code
    # more information about the code visit https://github.com/hexgrad/kokoro
    parser.add_argument('--lang', type=str, default='a',
                        help='Specify the language code to use for text-to-speech (e.g., a). Default is a.')

    # Optional Positional for specifying the output file
    parser.add_argument('output_file', type=str, nargs='?', default='output.wav',
                        help='The output wave file, when option play is not specify then the audio is save the output file instead.')

    args = parser.parse_args()

    # Validate input file existence
    if not os.path.isfile(args.input_file):
        print(f"Error: Input file '{args.input_file}' not found.")
        return

    # Read text from the input file
    try:
        with open(args.input_file, 'r', encoding='utf-8') as f:
            text_input = f.read()
    except Exception as e:
        print(f"Error reading input file: {e}")
        return

    print(f"Using voice: {args.voice}")
    if args.play:
        print("Audio will be played after generation.")
    else:
        if not args.output_file.endswith(".wav"):
            print(f"Output must be a wave file: {args.output_file}")
            return
        print(f"Output will be saved to: {args.output_file}")

    # Initialize Kokoro pipeline
    try:
        pipeline = KPipeline(lang_code=args.lang, repo_id='hexgrad/Kokoro-82M')  # Assuming American English
    except Exception as e:
        print(f"Error initializing Kokoro pipeline: {e}")
        return

    sentences = text_input.strip().split('\n')

    # Generate audio
    output_audios = []
    sample_rate = 24000

    for i, sentence in enumerate(sentences):
        if not sentence.strip(): # Skip empty lines
            continue

        try:
            generator = pipeline(
                text_input,
                voice=args.voice,
                # Add other Kokoro options here if needed, e.g., speed=1
            )

            for i, (gs, ps, audio) in enumerate(generator):
                output_audios.append(audio)

        except Exception as e:
            print(f"Error during audio generation: {e}")
            return

    if args.play:
        p = None
        stream = None
        try:
            p = pyaudio.PyAudio()
            # Assuming mono channel output from Kokoro
            # The format should match the numpy array dtype. paInt16 for int16, paFloat32 for float32
            # We'll convert to int16 for playback for broader compatibility
            stream = p.open(format=pyaudio.paFloat32,
                            channels=1, # Assuming mono
                            rate=sample_rate,
                            output=True)
            print("\nStarting real-time audio playback...")
        except ImportError:
            print("PyAudio not found. Cannot play audio. Install with: pip install pyaudio")
            return
        except Exception as py_e:
            print(f"Error initializing PyAudio stream: {py_e}")
            return

        combined_audio = np.concatenate(output_audios,axis=0).astype(np.float32)
        stream.write(combined_audio.tobytes());

        if stream:
            stream.stop_stream()
            stream.close()
        if p:
            p.terminate()

        return

    if not output_audios:
        print("No audio segments were generated. Exiting.")
        return

    try:
        combined_audio = np.concatenate(output_audios,axis=0).astype(np.float32)
        sf.write(args.output_file, combined_audio, sample_rate)
        print("\nAudio generation complete.")
    except Exception as e:
        print(f"Error saving combined audio: {e}")
        return

if __name__ == "__main__":
    main()
