{
  "targets": [
    {
      "target_name": "gitdist",
      "sources": [ "object_v8.cc", "gitdist.cc" ],
      "include_dirs":[
        "/home/ubuntu/libgit2-0.21.5/include"
      ],
      "link_settings": {
          "libraries":[
            "/home/ubuntu/libgit2-0.21.5/build/libgit2.a"
          ]
      }
    }
  ]
}
