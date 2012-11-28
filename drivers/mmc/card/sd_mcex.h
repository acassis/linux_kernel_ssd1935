//******************************************************************************
//*Copyright:     Shenzhen aigo R&D Co. Ltd.
////******************************************************************************
// Project name:  McEX driver
//******************************************************************************
//! @file
//!
//! @author       @$Id: sd_mcex.h,v 1.1 2009/02/04 06:31:59 zhuyuming_ssldev Exp $
//!
//! @brief        McEX driver
//!
//! @details      This module implements the Mobile Commerce Extension driver
//!               as an optional extension of the SD block device driver.
//!
//! @warning      The code is supported by the third part company. Use consistent
//!               with the GNU GPL is permitted,provided that this copyright notice
//!               is preserved in its entirety in all copies and derived works.
//******************************************************************************

#ifndef __SD_MCEX_H__
#define __SD_MCEX_H__


//***** Include files **********************************************************
#include <linux/ioctl.h>


//***** Constants **************************************************************

// McEX control TRM args
#define SD_MCEX_WARM_RESET             1     //!< Default arg for command CONTROL_TRM

// McEX registers
#define SD_MCEX_SR                     0     //!< McEX register SR (status register)
#define SD_MCEX_PR                     4     //!< McEX register PR (properties register)
#define SD_MCEX_RNR                    6     //!< McEX register RNR (random number register)

// IOCTL - magic number
#define SD_MCEX_IOCTL_MAGIC            0xBF  //!< IOCTL magic number

// IOCTL - definitions
#define SD_MCEX_IOCTL_SWITCH           _IO(SD_MCEX_IOCTL_MAGIC, 0)   //!< IOCTL - command SWITCH
#define SD_MCEX_IOCTL_CONTROL_TRM      _IO(SD_MCEX_IOCTL_MAGIC, 1)   //!< IOCTL - command CONTROL_TRM
#define SD_MCEX_IOCTL_SEND_PSI         _IO(SD_MCEX_IOCTL_MAGIC, 2)   //!< IOCTL - command SEND_PSI
#define SD_MCEX_IOCTL_WRITE_SEC_CMD    _IO(SD_MCEX_IOCTL_MAGIC, 3)   //!< IOCTL - command WRITE_SEC_CMD
#define SD_MCEX_IOCTL_READ_SEC_CMD     _IO(SD_MCEX_IOCTL_MAGIC, 4)   //!< IOCTL - command READ_SEC_CMD
#define SD_MCEX_IOCTL_WRITE_BLOCK      _IO(SD_MCEX_IOCTL_MAGIC, 5)   //!< IOCTL - command WRITE_BLOCK
#define SD_MCEX_IOCTL_READ_BLOCK       _IO(SD_MCEX_IOCTL_MAGIC, 6)   //!< IOCTL - command READ_BLOCK 

//! Macro to build the SWITCH arg
#define SWITCH_ARG(mode,group,value)   (int)(((((mode << 31) & 0x80000000) | 0x00FFFFFF) & (~(0xF << ((group-1) * 4)))) | (value << ((group-1) * 4)))


//***** Structures *************************************************************

//! McEX data structure
struct sd_mcex_data
{
   int arg;                      //!< command argument (optional)
   unsigned char buffer[512];    //!< command data (in or out)
   int error;                    //!< command error (0 in case of success)
};


#endif // __SD_MCEX_H__


//******************************************************************************
// End of file
//******************************************************************************
