import { writeFile } from "fs/promises";

function writeString(view, offset, string) {
  for (let i = 0; i < string.length; ++i) {
    view.setUint8(offset + i, string.charCodeAt(i));
  }
}

// create wave header file and combine with wave binary data
export function encodeWAV(samples, rate, channel, bits) {
  let offset = 44;
  const buffer = new ArrayBuffer(offset + samples.length * 4);
  const view = new DataView(buffer);

  /* RIFF identifier */
  writeString(view, 0, "RIFF");
  /* RIFF chunk length */
  view.setUint32(4, 36 + samples.length * 4, true);
  /* RIFF type */
  writeString(view, 8, "WAVE");
  /* format chunk identifier */
  writeString(view, 12, "fmt ");
  /* format chunk length */
  view.setUint32(16, 16, true);
  /* sample format (raw) */
  view.setUint16(20, 3, true);
  /* channel count */
  view.setUint16(22, channel, true);
  /* sample rate */
  view.setUint32(24, rate, true);
  /* byte rate (sample rate * block align) */
  view.setUint32(28, rate * 4, true);
  /* block align (channel count * bytes per sample) */
  view.setUint16(32, channel * 4, true);
  /* bits per sample */
  view.setUint16(34, bits, true);
  /* data chunk identifier */
  writeString(view, 36, "data");
  /* data chunk length */
  view.setUint32(40, samples.length * 4, true);

  for (let i = 0; i < samples.length; ++i, offset += 4) {
    view.setFloat32(offset, samples[i], true);
  }

  return buffer;
}

// saveWAV encode wave and save binary wave to a file
async function saveWAV(filename, samples, rate) {
  const buff = Buffer.from(encodeWAV(samples, rate));
  await writeFile(filename, buff);
}
