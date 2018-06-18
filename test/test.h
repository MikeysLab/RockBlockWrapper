#pragma once

#define MESSAGE_QUEUE_LENGTH      5
#define MESSAGE_LENGTH            100
#define I2C_SLAVE_ADDRESS         0x02

#define ERROR_NO_ERROR            -2
#define ERROR_QUEUE_FULL          -1
#define ERROR_NO_SAT              1
#define ERROR_NO_MODULE_COMM      2

#define MIN_TIME_BETWEEN_TRANSMIT 40000

#define MESSAGE_TYPE_TEXT         1
#define MESSAGE_TYPE_BINARY       2

#define MESSAGE_PRIORITY_NORMAL   0
#define MESSAGE_PRIORITY_HIGH     1
#define MESSAGE_PRIORITY_CRITICAL 2

#define MESSAGE_STATUS_NONE       0
#define MESSAGE_STATUS_SENT       1
#define MESSAGE_STATUS_QUEUED     2
#define MESSAGE_STATUS_ERROR      3

#define RB_SLEEP_PIN              5
#define RB_SAT_INT_PIN            3
#define RB_TX_PIN                 10
#define RB_RX_PIN                 11
#define TEST_INT_PIN              2

#define RB_WAKEUP_CHARGE_TIMEOUT  2000