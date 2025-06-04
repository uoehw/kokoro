import { KokoroWrapper } from "./kokoro.js";
import yargs from "yargs";
import { hideBin } from "yargs/helpers";

const argv = yargs(hideBin(process.argv))
  .option("play", {
    type: "boolean",
    description: "Enables playback",
  })
  .option("lang", {
    type: "string",
    description: "language to be use for tts (not use, it selected via voice)",
  })
  .option("voice", {
    type: "string",
    description: "voice to be use for tts",
  })
  .help().argv;

if (
  !argv._ ||
  (argv._.length !== 1 && argv.play === true) ||
  (argv._.length !== 2 && !argv.play)
) {
  console.log("Supported command:");
  console.log("npm run kokoro -- --play --voice=af_heart input.txt");
  console.log("npm run kokoro -- --voice=af_heart input.txt output.wav");
  process.exit(1);
}

if (argv.play) {
  (async () => {
    process.on("uncaughtException", (err) => {
      console.log(err);
    });
    await KokoroWrapper.ttsFromFile(argv._[0], {
      play: true,
      voice: argv.voice,
      lang: argv.lang,
    });
  })();
} else if (argv._[1].endsWith(".wav")) {
  (async () => {
    await KokoroWrapper.ttsFromFile(argv._[0], {
      filename: argv._[1],
      voice: argv.voice,
      lang: argv.lang,
    });
  })();
} else {
  console.log("output file should have exentsion .wav");
}
