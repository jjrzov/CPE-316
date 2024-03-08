/**
******************************************************************************
* @file    spsgrf.c
* @author  Matthew Mielke
* @version V1.0.0
* @date    07-Jul-2021
* @brief   An abstraction of the SPIRIT1 library for the SPSGRF module.
******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include "spsgrf.h"


/* External functions --------------------------------------------------------*/
/**
* @brief  Initialize the SPIRIT1 transceiver in the SPSGRF module.
* @param  None
* @retval None
*/
void SPSGRF_Init(void)
{
  SRadioInit xRadioInit;
  PktBasicInit xBasicInit;
  PktBasicAddressesInit xBasicAddress;
  SGpioInit xGpioInit;

  SpiritSpiInit();

  // restart the radio
  SpiritEnterShutdown();
  SpiritExitShutdown();
  SpiritManagementWaExtraCurrent(); // To be called at the SHUTDOWN exit. It avoids extra current consumption at SLEEP and STANDBY.

  // wait for the radio to enter the ready state
  do
  {
    for (volatile uint8_t i = 0; i != 0xFF; i++); // delay for state transition
    SpiritRefreshStatus(); // reads the MC_STATUS register
  } while (g_xStatus.MC_STATE != MC_STATE_READY);

  // Initialize radio RF parameters
  xRadioInit.nXtalOffsetPpm = XTAL_OFFSET_PPM;
  xRadioInit.lFrequencyBase = BASE_FREQUENCY;
  xRadioInit.nChannelSpace = CHANNEL_SPACE;
  xRadioInit.cChannelNumber = CHANNEL_NUMBER;
  xRadioInit.xModulationSelect = MODULATION_SELECT;
  xRadioInit.lDatarate = DATARATE;
  xRadioInit.lFreqDev = FREQ_DEVIATION;
  xRadioInit.lBandwidth = BANDWIDTH;
  SpiritRadioSetXtalFrequency(XTAL_FREQUENCY); // Must be called before SpiritRadioInit()
  SpiritRadioInit(&xRadioInit);

  // Set the transmitter power level
  SpiritRadioSetPALeveldBm(POWER_INDEX, POWER_DBM);
  SpiritRadioSetPALevelMaxIndex(POWER_INDEX);

  // Configure packet handler to use the Basic packet format
  xBasicInit.xPreambleLength = PREAMBLE_LENGTH;
  xBasicInit.xSyncLength = SYNC_LENGTH;
  xBasicInit.lSyncWords = SYNC_WORD;
  xBasicInit.xFixVarLength = LENGTH_TYPE;
  xBasicInit.cPktLengthWidth = LENGTH_WIDTH;
  xBasicInit.xCrcMode = CRC_MODE;
  xBasicInit.xControlLength = CONTROL_LENGTH;
  xBasicInit.xAddressField = EN_ADDRESS;
  xBasicInit.xFec = EN_FEC;
  xBasicInit.xDataWhitening = EN_WHITENING;
  SpiritPktBasicInit(&xBasicInit);

  // Configure destination address criteria for automatic packet filtering
  xBasicAddress.xFilterOnMyAddress = EN_FILT_MY_ADDRESS;
  xBasicAddress.cMyAddress = MY_ADDRESS;
  xBasicAddress.xFilterOnMulticastAddress = EN_FILT_MULTICAST_ADDRESS;
  xBasicAddress.cMulticastAddress = MULTICAST_ADDRESS;
  xBasicAddress.xFilterOnBroadcastAddress = EN_FILT_BROADCAST_ADDRESS;
  xBasicAddress.cBroadcastAddress = BROADCAST_ADDRESS;
  SpiritPktBasicAddressesInit(&xBasicAddress);

  // Configure GPIO3 as interrupt request pin (active low)
  xGpioInit.xSpiritGpioPin = SPIRIT_GPIO_3;
  xGpioInit.xSpiritGpioMode = SPIRIT_GPIO_MODE_DIGITAL_OUTPUT_LP;
  xGpioInit.xSpiritGpioIO = SPIRIT_GPIO_DIG_OUT_IRQ;
  SpiritGpioInit(&xGpioInit);

  // Generate an interrupt request for the following IRQs
  SpiritIrqDeInit(NULL);
  SpiritIrq(TX_DATA_SENT, S_ENABLE);
  SpiritIrq(RX_DATA_READY, S_ENABLE);
  SpiritIrq(RX_DATA_DISC, S_ENABLE);
  SpiritIrq(RX_TIMEOUT, S_ENABLE);
  SpiritIrqClearStatus();

  // Enable the synchronization quality indicator check (perfect match required)
  // NOTE: 9.10.4: "It is recommended to always enable the SQI check."
  SpiritQiSetSqiThreshold(SQI_TH_0);
  SpiritQiSqiCheck(S_ENABLE);

  // Set the RSSI Threshold for Carrier Sense (9.10.2)
  // NOTE: CS_MODE = 0 at reset
  SpiritQiSetRssiThresholddBm(RSSI_THRESHOLD);

  // Configure the RX timeout
#ifdef RECEIVE_TIMEOUT
  SpiritTimerSetRxTimeoutMs(RECEIVE_TIMEOUT);
#else
  SET_INFINITE_RX_TIMEOUT();
#endif /* RECIEVE_TIMEOUT */
  SpiritTimerSetRxTimeoutStopCondition(SQI_ABOVE_THRESHOLD);
}


/**
* @brief  Performs a transmission by loading the TX FIFO with the data to be
*         sent, setting the payload length, and strobing the TX command.
* @param  txBuff: pointer to the data to transmit
* @param  txLen: number of bytes to transmit
* @retval None
*/
void SPSGRF_StartTx(uint8_t *txBuff, uint8_t txLen)
{
  //Fix state
	SpiritCmdStrobeSabort();

	// flush the TX FIFO
  SpiritCmdStrobeFlushTxFifo();

  // Avoid TX FIFO overflow
  txLen = (txLen > MAX_BUFFER_LEN ? MAX_BUFFER_LEN : txLen);

  // start TX operation
  SpiritSpiWriteLinearFifo(txLen, txBuff);
  SpiritPktBasicSetPayloadLength(txLen);
  SpiritCmdStrobeTx();
}


/**
* @brief  Enter the receive state.
* @param  None
* @retval None
*/
void SPSGRF_StartRx(void)
{
//Fix state
	SpiritCmdStrobeSabort();
  SpiritCmdStrobeRx();
}


/**
* @brief  To be called after a reception is complete
* @param  rxBuff: pointer to a buffer to hold the received data
* @retval Number of bytes received
*/
uint8_t SPSGRF_GetRxData(uint8_t *rxBuff)
{
  uint8_t len;

  len = SpiritLinearFifoReadNumElementsRxFifo();
  SpiritSpiReadLinearFifo(len, rxBuff);

  return len;
}

/*** end of file ***/
