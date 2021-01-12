# esp-idf-telegram-bot


## Overview

This code let you send the method `getMe` and `sendMessage` to a telegram bot using https get and https post respectively. Using the `esp_http_client` example to begging and kenzanin's repository called `esp-idf-telegrambot` as guide I was able to achieve communication with telegram server. The certificate (that let you use https) has been change to the one in `https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot/blob/master/src/TelegramCertificate.h` in order to communicate with the telegram server.

It work with the function `esp_http_client_perform`, but it has another path with the native example, but this then to crash and force the ESP32 to reboot.

## Installation

* Download the latest realase with `git clone https://github.com/antusystem/esp-idf-telegram-bot.git`
* Go to the folder cd ../esp-idf-telegram-bot.git
* Build the menuconfig to make the respective changes to your computer
* If you are using Eclipse, once it is added as a project, in its propierties check C/C++ Build > Environment check the IDF_PATH if it is correct
* Compile `make clean`
* Compile `make all`

It should not indicate any error following this steps.

### Configure the project

You have to configurate the next **defines** for it to work:

- **ESP_WIFI_SSID**: you have to introduce your ssid to connect to your wifi network.
- **ESP_WIFI_PASS**: you have to wrtie your ssid password to connect to your wifi network.
- **API_KEY**: the API key that Bot Father already gave you when you create your bot.
- **chat_ID1**: if you know the name of a publi chat write it in this define.
- **chat_ID2**: to write yo any type of chat, write its ID in here. This one work for both private and public groups.
- **LED**: configure if you want to turn on a led connected to a pin.

With this it should work fine and write to the chat (personal or to a group) in the id.

## Troubleshooting

Check de IDFPATH in the configurations of the proyect

## Log

* Last compile: January 11th, 2021.
* Last test: January 11th, 2021.
* Last compile espidf version: v4.2

## License
MIT License

Copyright (c) [2020] [antusystem]

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
