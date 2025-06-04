{
  'targets': [
    {
      'target_name': 'alsa_player',
      'sources': [ 'alsa/alsa_player.cc' ], # Your C++ source file
      'libraries': [ '-lasound' ],        # Link against ALSA library
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ],
      'include_dirs': [
        "<!@(node -p \"require('node-addon-api').include\")"
      ]
    }
  ]
}
