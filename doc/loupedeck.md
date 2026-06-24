Boatswain uses USB bulk in and out transfers to control Loupedeck-based devices.

Logitech has discontinued Loupedeck devices.

# Prelude

This document will not go through explaining every USB terminology, refer to the USB documentations mainly about the CDC devices and PSTN devices ([link][1]).

Loupedeck-based devices are identified as CDC-ACM devices ("USB serial").
The Linux kernel will automatically attach the `cdc-acm` module.
In the userspace, sending correctly formed USB bulk transfer is enough.

Loupedeck first devices (CT/Live) were behaving like network interfaces, they changed it through firmware to behave as CDC-ACM devices ([source][2] [[archive][3]]).

Newer model were directly behaving as the later.

NOTE: Only the Razer Stream Controller X with Loupedeck software has been used to write this document alongside third-party projects in Javascript.

# Protocol

Unless specified:
- Sending (request) to the device means USB bulk out transfer
- Receiving (answer, respond) from the device means USB bulk in transfer
- Sending to the host means USB bulk in transfer

## Initialization

To be able to use the device, the host needs to send a request to switch protocol:

```c
"GET /index.html\r\n"
"HTTP/1.1\r\n"
"Upgrade: websocket\r\n"
"Sec-WebSocket-Key: 123abc\r\n"
"\r\n"
```

Firstly, no the protocol we are switching to is not WebSocket.
We can speculate that it is an artifact from the "network interface" era.

The "WebSocket" key does not matter.

The code snippet is written in C, no NULL-termination character should be sent.

The device should answer with a response similar to this:

```c
"HTTP/1.1 101 Switching Protocols\r\n"
"Upgrade: websocket\r\n"
"Connection: Upgrade\r\n"
"Sec-WebSocket-Accept: ALtlZo9FMEUEQleXJmq++ukUQ1s=\r\n"
"\r\n"
```

The received buffer is not NULL-terminated by the device.

The host should:

1. Send a control transfer to set the control line to zero (deactivate carrier and DTE not present).
2. Send a control transfer to set the line coding to 9600 bit/s, 2 stop bits, no parity and 8 data bits.

To be ensure to be able to finalize the device (restore its previous protocol), four send break control transfer should be done with the following value for each transfer:

1. `0xFFFF`
2. `0x0000`
3. `0xFFFF`
4. `0x0000`

From here the device will send and expect to receive payloads (not human readable).

## Payloads

When you send a command to the device or when the device send information it will usually be done as two transfer, two payloads:

### First payload

The first payload is composed of two information:
1. The magic number `0x82` (1 byte)
2. The length of the next payload with some variation

#### Device to host

NOTE: This section is missing documentation about large sized response

1. The size of the length is one byte
2. The max length of the next payload could be 255 but it was not tested
   - Most of [payload types](#payload-types) will return a small sized information

Example: `0x82 0x13`

#### Host to device

Depending on the size of the second payload the format of the payload will change:

##### The size is inferior or equal to 126 (0x7E)

1. The size of the length is 7 bits
2. The value of the size is added to `0x80` (`0x80 + 0x03 = 0x83`)
3. 4 trailing empty bytes

Example: `0x82 0x83 0x00 0x00 0x00 0x00`

##### The size is superior or equal to 127

1. The "original" size byte is set to `0xFF` followed with 4 empty bytes
2. The size of the length of the next payload is 4 bytes (unsigned integer 32BE)
3. The max length of the next payload is technically 2³²-1
4. 4 trailing empty bytes

Example: `0x82 0xFF 0x00 0x00 0x00 0x00 0x10 0x10 0x10 0x10 0x00 0x00 0x00 0x00`

### Second payload(s)

The second payload is composed of 4 information:

1. The length of the payload or `0xFF` as 1 byte
2. The identifier of the type of payload
3. The transaction id
4. The content (if there is) of the payload

This payload can be split in multiple bulk transfer, e.g. the buffer is usually sent as separated from the rest.

#### Device to host

The transaction id is non-null only if it's a response from a previous request made with the same id made by the host.

## Finalization

Send four send break control transfer with the following value for each transfer:

1. `0xFFFF`
2. `0x0000`
3. `0xFFFF`
4. `0x0000`

The device should have reverted to its protocol before the switch.

## Payload types

### `0x03` Get serial number

- No content needs to be sent in the request payload
- Returns the serial number string with trailing white-spaces and is **not** NULL-terminated

### `0x07` Get firmware version

- No content needs to be sent in the request payload
- Returns a semver-like version with one number per bytes

### `0x09` Set brightness

- Brightness level as an integer between 0 and 10, 1 byte
- Returns `0x01` if nothings gone wrong

### `0x0D` Get "MCU Identifier" (not confirmed)

This type requires more observation and documentation since this one returns more than 2 bulk in transfer as response

### `0x0F` Apply framebuffer

A setup framebuffer should be sent before send one of this type.

- An identifier indication which display to target and the used format as 2 bytes
- Should return `0x00` if nothings gone wrong

### `0x10` Setup framebuffer

- An identifier indication which display to target and the used format as 2 bytes
- The X position where the drawing should start as an unsigned integer 16bits BE
- The Y position where the drawing should start as an unsigned integer 16bits BE
- The width of the drawing as an unsigned integer 16bits BE
- The height of the drawing as an unsigned integer 16bits BE
- Image pixel bytes in the matching format
  - Can be sent in a separate bulk in transfer
- Should return `0x01` if nothings gone wrong

### `0x01f` Clear

- No content needs to be sent in the request payload.
- Returns `0x01` if nothings gone wrong

### `0x73`

The device send some of those just after switching the protocol.

Those look like gibberish based on previously sent data (e.g. protocol switch response) and so can be ignored.

Usually 8 of those are sent.

# Device specificity

## Razer Stream Controller X

### Button grid

This device has 15 see-through buttons with a screen behind.

The screen is 480x288, but only 480x270 is used when observing captured frame from the Loupedeck Software.

The device seems to support B5G6R5 when using a specific id `0x4d` (or `'M'`).

But the Loupedeck Software sends R8G8B8 on the id `0x38`.

Measure deduces from putting the captured buffer in GIMP as raw data (RGB with 480 as width):

- Button images have a size of 72x72
- 9px offset from left border, same for right
- 2px offset from the top (down is 2 + 18)
- Rows are spaced of 25px from each other
- Columns 0 and 1 are spaced of 26px
- Columns 1 and 2 are spaced of 25px
- Columns 2 and 3 are spaced of 26px
- Columns 3 and 4 are spaced of 25px

Schema:

||**---**|**---**|**---**|**---**|**2**|**---**|**---**|**---**|**---**||
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|**\|**|[B]|**\|**|[B]|**\|**|[B]|**\|**|[B]|**\|**|[B]|**\|**|
|**9**|[B]|**<-26->**|[B]|**<-25->**|[B]|**<-26->**|[B]|**<-25->**|[B]|**9**|
|**\|**|[B]|**\|**|[B]|**\|**|[B]|**\|**|[B]|**\|**|[B]|**\|**|
||**---**|**---**|**---**|**---**|**2 + 18**|**---**|**---**|**---**|**---**||


[1]: https://www.usb.org/document-library/class-definitions-communication-devices-12
[2]: https://support.loupedeck.com/firmware
[3]: https://web.archive.org/web/20250122010429/https://support.loupedeck.com/firmware


