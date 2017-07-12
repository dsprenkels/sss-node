{
  "targets": [
    {
      "target_name": "shamirsecretsharing",
      "cflags": ["-g", "-Wall", "-Wno-sign-compare"],
      "cflags_cc": ["-g", "-Wall", "-fexceptions"],
      "sources": [
        "shamirsecretsharing.cc",
        "sss/randombytes/randombytes.c",
        "sss/sss.c",
        "sss/hazmat.c",
        "sss/tweetnacl.c"
      ],
      "include_dirs" : [
        "<!(node -e \"require('nan')\")"
      ]
    }
  ]
}
