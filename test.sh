#! /usr/bin/bash
echo -e "GET / HTTP/1.0\n\n" | socat - TCP:localhost:8080
echo -e "GET /quit HTTP/1.0\n\n" | socat - TCP:localhost:8080
