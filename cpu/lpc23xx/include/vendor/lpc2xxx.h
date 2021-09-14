#include <stdint.h>

/**
 * @brief   Type for 32-bit registers
 */
#define REG32       volatile uint32_t

/**
 * @brief   Type for 16-bit registers
 */
#define REG16       volatile uint16_t

/**
 * @brief   Type for 8-bit registers
 */
#define REG8        volatile uint8_t

/**
 * @brief   Generic timer register map
 */
typedef struct {
    REG32   IR;             /**< interrupt register */
    REG32   TCR;            /**< timer control register */
    REG32   TC;             /**< timer counter */
    REG32   PR;             /**< prescale register */
    REG32   PC;             /**< prescale counter */
    REG32   MCR;            /**< match control register */
    REG32   MR[4];          /**< match registers 1-4 */
    REG32   CCR;            /**< capture control register */
    REG32   CR[4];          /**< capture register 1-4 */
    REG32   EMR;            /**< external match register */
    REG32   reserved[12];   /**< reserved */
    REG32   CTCR;           /**< count control register */
} lpc23xx_timer_t;

/**
 * @brief   Generic UART register map
 */
typedef struct {
    union {
        REG32   RBR;            /**< Receiver Buffer Register  */
        REG32   THR;            /**< Transmit Holding Register */
        REG8    DLL;            /**< Divisor Latch LSB         */
    };
    union {
        REG32   IER;            /**< Interrupt Enable Register */
        REG8    DLM;            /**< Divisor Latch MSB         */
    };
    union {
        REG32   IIR;            /**< Interrupt ID Register     */
        REG32   FCR;            /**< FIFO Control Register     */
    };
    REG32   LCR;                /**< Line Control Register     */
    REG32   MCR;                /**< Modem Control Register    */
    REG32   LSR;                /**< Line Status Register      */
    REG32   MSR;                /**< Modem Status Register     */
    REG32   SCR;                /**< Scratch Pad Register      */
    REG32   ACR;                /**< Auto-baud Control Register*/
    REG32   ICR;                /**< IrDA Control Register     */
    REG32   FDR;                /**< Fractional Divider        */
    REG32   reserved;           /**< unused                    */
    REG8    TER;                /**< Transmit Enable Register  */
} lpc23xx_uart_t;


/**
 * @brief   Generic I2C register map
 */
typedef struct {
    REG32   CONSET;         /**< Control Set Register       */
    REG32   STAT;           /**< Status Register            */
    REG32   DAT;            /**< Data Register              */
    REG32   ADR;            /**< Slave Address Register     */
    REG32   SCLH;           /**< Duty Cycle High Half Word  */
    REG32   SCLL;           /**< Duty Cycle Low Half Word   */
    REG32   CONCLR;         /**< Control Clear Register     */
} lpc23xx_i2c_t;

/**
 * @brief   Generic SPI register map
 */
typedef struct {
    REG32 CR0;              /**< Control Register 0                 */
    REG32 CR1;              /**< Control Register 1                 */
    REG32 DR;               /**< Data Register                      */
    REG32 SR;               /**< Status Register                    */
    REG32 CPSR;             /**< Clock Prescale Register            */
    REG32 IMSC;             /**< Interrupt Mask Set/Clear Register  */
    REG32 RIS;              /**< Raw Interrupt Status Register      */
    REG32 MIS;              /**< Masked Interrupt Status Register   */
    REG32 ICR;              /**< Interrupt Clear Register           */
    REG32 DMACR;            /**< DMA Control Register               */
} lpc23xx_spi_t;
