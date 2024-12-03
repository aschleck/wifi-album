# 10" Inkplate wifi album thingy

## Is it good?

No

## How do I run it?

Something like this maybe

```
arduino-cli core update-index -v
arduino-cli core install Inkplate_Boards:esp32
vim ./inkplate/inkplate.ino # change SSID and password to your network
cd ./inkplate && make upload
cd ./client && cargo build
./rotate.sh <inkplate-ip>:9876 ~/photo
```
