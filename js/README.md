# Requirement

On linux Debian/Ubuntu like OS install the below package.

```shell
sudo apt-get install libasound2-dev
```

Install node dependency

```shell
npm install --onnxruntime-node-install-cuda=skip
```

Build linux binding

```shell
./node_modules/.bin/node-gyp configure
./node_modules/.bin/node-gyp build
```

# Run TTS

Save audio output

```shell
npm run kokoro -- --voice=af_heart audio.en.txt output.wav
npm run kokoro -- --voice=zf_xiaobei audio.ch.txt output.wav
npm run kokoro -- --voice=jf_alpha audio.ja.txt output.wav
npm run kokoro -- --voice=ef_dora audio.es.txt output.wav
```

Play audio output

```shell
npm run kokoro -- --play --voice=af_heart audio.en.txt
npm run kokoro -- --play --voice=zf_xiaobei audio.ch.txt
npm run kokoro -- --play --voice=jf_alpha audio.ja.txt
npm run kokoro -- --play --voice=ef_dora audio.es.txt
```
