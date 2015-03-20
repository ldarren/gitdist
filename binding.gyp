{
  "targets": [
    {
      "target_name": "gitdist",
      "sources": [ "object_v8.cc", "repository_v8.cc", "gitdist.cc" ],
      "include_dirs":[
        "/home/ubuntu/libgit2-0.22.1/include"
      ],
      "link_settings": {
          "libraries":[
            "/home/ubuntu/libgit2-0.22.1/build/libgit2.a"
          ]
      },
      "ldflags": [
        "-lssh2",
        "-lssl"
      ]
    }
  ]
}
