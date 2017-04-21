Idea is to have two devices:
 * Sender - reads from Serial RX and sends via NRF24 to Receiver
 * Receiver - reads from NRF24 and sends to Serial TX

Also there is two way communication:
 * Two_way - don't forget to change role before uploading
 TODO: would be nice to add Reset button functionality - simply add button on Master and add Reset out on Slave
	Send system packet when detected reset command on Master, so device connected to Slave will receive reset signal
	This can be used to upload sketch on remote device