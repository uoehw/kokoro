# Requirement

Install latest [espeak-ng](https://github.com/espeak-ng/espeak-ng/blob/master/docs/building.md) from source, following the instruction on github page.

1. Create python virtual environment

   ```shell
   python3 -m venv venv
   ```

2. Activate virtual environment. Node that, everytime you want to running python code in this repo, you will need to activate virtual environment via the command below.

   ```shell
   source venv/bin/activate
   ```

3. Install python dependency

   ```shell
   pip install kokoro
   pip install soundfile
   pip install pyopenjtalk
   pip install 'fugashi[unidic]'
   python -m unidic download
   pip install jaconv
   pip install mojimoji
   pip install ordered_set
   pip install pypinyin
   pip install cn2an
   pip install jieba
   pip install --upgrade kokoro
   sudo apt install portaudio19-dev
   pip install pyaudio
   ```

> Note the package `pyopenjtalk`, `fugashi`, `jaconv`, `mojimoji` is required for japanese language. The english and spanish language does not required these packages.

> Note the package `pypinyin`, `cn2an`, `ordered_set`, `jieba` is required for chinese language. The english and spanish language does not required these packages.

# Test

Save audio file

```shell
python3 example.py --voice=af_heart --lang=a ../text/audio.en.txt
python3 example.py --voice=ef_dora --lang=e ../text/audio.es.txt
python3 example.py --voice=jf_alpha --lang=j ../text/audio.ja.txt
python3 example.py --voice=zf_xiaobei --lang=z ../text/audio.ch.txt
```

Play audio

```shell
python3 example.py --play --voice=af_heart --lang=a ../text/audio.en.txt
python3 example.py --play --voice=ef_dora --lang=e ../text/audio.es.txt
python3 example.py --play --voice=jf_alpha --lang=j ../text/audio.ja.txt
python3 example.py --play --voice=zf_xiaobei --lang=z ../text/audio.ch.txt
```

# TODO:

- Split task between reading text and write audio to stream when playing audio on the flight to reduce waiting time if text were too long.
