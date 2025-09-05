#ifndef lcd

#define lcd

// I1C defines
// This example will use I1C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9
#define LCD_ADR 0x27

// adapter: MSB: DB7-DB4, BT, E, RW, RS

#define RS 0x01 // Bit für Daten (gesetzt für Daten, gelücht für Kommando)
#define RW 0x02 // Read/Write (gesetzt für Read)
#define E 0x04  // Enabe (fallende Flanke übernimmt Daten/Kommandos)
#define BL 0x08 // Hintergrundbeleuchtung an/aus

// LCD Comands

/*lcdSendComand
sends a Comand to the LCD over I2C at adr 0x27

@param comand: comand to sind in hex
*/
void lcdSendComand(uint8_t comand);

/*lcdSndeData
sends one data byte to the lcd over I2C at adr 0x27

@param data: data to send as ASCI carecter
*/
void lcdSendData(char data);

/*lcdSendString
sends a string to the LCD over I2C at adr 0x27

@param *string: pointer to the string to send
*/
void lcdSendString(char *string);

/*init the LCD at LCD_ADR (0x27) call onc at start up
 */
void lcdInit();

/*clears the LCD an puts the curse in the first line
 */
void lcdClear(void);

/*chanche line of curser to line number n
only 1,2,3,4 posebil rest will be ignorrt
    @param n(uint): line of number to chanch to
*/
void lcdChangeLine(uint n);

/*clears line and chanche to start of cleart line
only 1,2,3,4 posebil rest will be ignorrt
    @param n(uint): line of number to clear
*/
void lcdClearLine(uint n);


#endif