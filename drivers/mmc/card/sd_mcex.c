//******************************************************************************
//*Copyright: 	  Shenzhen aigo R&D Co. Ltd.
//******************************************************************************
// Project name:  McEX driver
//******************************************************************************
//! @file
//!
//! @author       @$Id: sd_mcex.c,v 1.1 2009/02/04 06:31:58 zhuyuming_ssldev Exp $
//!
//! @brief        McEX driver
//!
//! @details      This module implements the Mobile Commerce Extension driver
//!               as an optional extension of the SD block device driver.
//!
//! @warning      The code is supported by the third part company. Use consistent 
//!		  with the GNU GPL is permitted,provided that this copyright notice 
//!		  is preserved in its entirety in all copies and derived works.
//******************************************************************************


//***** Include files **********************************************************
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/scatterlist.h>
#include <asm/uaccess.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include "sd_mcex.h"
//#define DEBUG   // uncomment this line to include debug trace


//***** Constants **************************************************************

// McEX commands
#define SD_MCEX_READ_SEC_CMD     34    //!< McEX command READ_SEC_CMD
#define SD_MCEX_WRITE_SEC_CMD    35    //!< McEX command WRITE_SEC_CMD
#define SD_MCEX_SEND_PSI         36    //!< McEX command SEND_PSI
#define SD_MCEX_CONTROL_TRM      37    //!< McEX command CONTROL_TRM

#define DIR_READ                 0     //!< Data exchange direction - from SD card to host controller
#define DIR_WRITE                1     //!< Data exchange direction - from host controller to SD card

#define RETRY                    6     //!< Number of retries


//******************************************************************************
//! @brief     This function sends a SD command without data exchange.
//!
//! @param[in] card the mmc_card handle
//! @param[in] opcode the SD command opcode (0..63)
//! @param[in] arg the SD command argument (32-bit)
//! @param[in] flags the SD command flags (see constants in ./include/linux/mmc/core.h)
//! @param[out] status the SD response status (32-bit)
//!
//! @return    0 in case of success, error code otherwise
//!
//! @warning   None
//******************************************************************************
int sd_mcex_send_command(struct mmc_card *card, int opcode, int arg, int flags, int* status)
{
   struct mmc_command cmd;
   int error;

	BUG_ON(!card);
	BUG_ON(!card->host);
	WARN_ON(!card->host->claimed);

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = opcode;
	cmd.arg = arg;
	cmd.flags = flags;
	
	error = mmc_wait_for_cmd(card->host, &cmd, RETRY);
	
	*status = cmd.resp[0];

#ifdef DEBUG
   printk(KERN_DEBUG "McEX : CMD%02d, arg=%08X, error=%d, status=%08X\n", opcode, arg, error, *status);
#endif // DEBUG
	return error;
}


//******************************************************************************
//! @brief     This function sends a SD command with data exchange (in or out).
//!
//! @param[in] card the mmc_card handle
//! @param[in] opcode the SD command opcode (0..63)
//! @param[in] arg the SD command argument (32-bit)
//! @param[in] flags the SD command flags (see constants in ./include/linux/mmc/core.h)
//! @param[out] status the SD response status (32-bit)
//! @param[in] direction either DIR_READ or DIR_WRITE
//! @param[in,out] buffer the data buffer
//! @param[in] length the data length
//!
//! @return    0 in case of success, error code otherwise
//!
//! @warning   None
//******************************************************************************
int sd_mcex_send_command_with_data(struct mmc_card *card, int opcode, int arg, int flags, int* status, int direction, u8 *buffer, int length)
{
	struct mmc_request mrq;
	struct mmc_command cmd;
	struct mmc_data data;
	struct scatterlist sg;
	int error;

	BUG_ON(!card);
	BUG_ON(!card->host);
	WARN_ON(!card->host->claimed);

	memset(&mrq, 0, sizeof(struct mmc_request));
	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));

	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.opcode = opcode;
	cmd.arg = arg;
	cmd.flags = flags;
	cmd.retries = RETRY;

   data.blksz = length;
   data.blocks = 1;
   data.flags = (direction == DIR_READ) ? MMC_DATA_READ : MMC_DATA_WRITE;
   data.sg = &sg;
   data.sg_len = 1;

   // NOTE: caller guarantees buffer is heap-allocated

	sg_init_one(&sg, buffer, length);

   mmc_set_data_timeout(&data, card);

	mmc_wait_for_req(card->host, &mrq);

	if (cmd.error)
		error = cmd.error;
	else if (data.error)
		error = data.error;
   else
   {
#ifdef DEBUG
      print_hex_dump(KERN_DEBUG, "McEX : ", DUMP_PREFIX_OFFSET, 16, 1, (void *)buffer, length, 1);
#endif // DEBUG
      error = 0;
   }      

	*status = cmd.resp[0];

#ifdef DEBUG
   printk(KERN_DEBUG "McEX : CMD%02d, arg=%08X, error=%d, status=%08X\n", opcode, arg, error, *status);
#endif // DEBUG
	return error;
}


//******************************************************************************
//! @brief     This function sends the SEND_STATUS command to the SD card and
//!            checks the status until the card is ready for data transfer.
//!
//! @param[in] card the mmc_card handle
//!
//! @return    0 in case of success, error code otherwise
//!
//! @warning   None
//******************************************************************************
int sd_mcex_wait_data_ready(struct mmc_card *card)
{
   int error, status;

#ifdef DEBUG
   printk(KERN_DEBUG "McEX : send command SEND_STATUS\n");
#endif // DEBUG
	
   do
   {
      error = sd_mcex_send_command(card, MMC_SEND_STATUS, card->rca << 16, MMC_RSP_R1 | MMC_CMD_AC, &status);
      if (error != 0)
         return error;
	}
	while (!(status & R1_READY_FOR_DATA) || (R1_CURRENT_STATE(status) == 7));

   return 0;
}


//******************************************************************************
//! @brief     This function implements the SD_MCEX_IOCTL_SWITCH operation.
//!
//! @param[in] card the mmc_card handle
//! @param[in] arg the pointer to a sd_mcex_data structure in user space
//!            @li ((struct sd_mcex_data *)arg)->arg = switch argument (in)
//!            @li ((struct sd_mcex_data *)arg)->buffer = 64-byte switch response (out)
//!            @li ((struct sd_mcex_data *)arg)->error = command error (out)
//!
//! @return    0 in case of success, error code otherwise
//!
//! @warning   None
//******************************************************************************
int sd_mcex_ioctl_switch(struct mmc_card *card, unsigned long arg)
{
   struct sd_mcex_data *data;
   int ret, status;

   data = kmalloc(sizeof(struct sd_mcex_data), GFP_KERNEL);
   if (!data)   
      return -ENOMEM;
   
   if (copy_from_user(data, (void __user *)arg, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }
   
   mmc_claim_host(card->host);
   
#ifdef DEBUG
   printk(KERN_DEBUG "McEX : send command SWITCH (arg=%08X)\n", data->arg);
#endif // DEBUG
   data->error = sd_mcex_send_command_with_data(card, SD_SWITCH, data->arg, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC, &status, DIR_READ, data->buffer, 64);

   sd_mcex_wait_data_ready(card);
   mmc_release_host(card->host);

   if (copy_to_user((void __user *)arg, data, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   ret = 0;   

out:   
   kfree(data);
   return ret;
}


//******************************************************************************
//! @brief     This function implements the SD_MCEX_IOCTL_CONTROL_TRM operation.
//!
//! @param[in] card the mmc_card handle
//! @param[in] arg the pointer to a sd_mcex_data structure in user space
//!            @li ((struct sd_mcex_data *)arg)->arg = control argument (in)
//!            @li ((struct sd_mcex_data *)arg)->buffer = unused
//!            @li ((struct sd_mcex_data *)arg)->error = command error (out)
//!
//! @return    0 in case of success, error code otherwise
//!
//! @warning   None
//******************************************************************************
int sd_mcex_ioctl_control_trm(struct mmc_card *card, unsigned long arg)
{
   struct sd_mcex_data *data;
   int ret, status;

   data = kmalloc(sizeof(struct sd_mcex_data), GFP_KERNEL);
   if (!data)   
      return -ENOMEM;
   
   if (copy_from_user(data, (void __user *)arg, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   mmc_claim_host(card->host);
   
#ifdef DEBUG
   printk(KERN_DEBUG "McEX : send command CONTROL_TRM (arg=%08X)\n", data->arg);
#endif // DEBUG
   data->error = sd_mcex_send_command(card, SD_MCEX_CONTROL_TRM, data->arg, MMC_RSP_SPI_R1 | MMC_RSP_R1B | MMC_CMD_AC, &status);
   
   sd_mcex_wait_data_ready(card);
   mmc_release_host(card->host);

   if (copy_to_user((void __user *)arg, data, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   ret = 0;   

out:
   kfree(data);
   return ret;
}


//******************************************************************************
//! @brief     This function implements the SD_MCEX_IOCTL_SEND_PSI operation.
//!
//! @param[in] card the mmc_card handle
//! @param[in] arg the pointer to a sd_mcex_data structure in user space
//!            @li ((struct sd_mcex_data *)arg)->arg = register ID (in)
//!            @li ((struct sd_mcex_data *)arg)->buffer = 32-byte register value (out)
//!            @li ((struct sd_mcex_data *)arg)->error = command error (out)
//!
//! @return    0 in case of success, error code otherwise
//!
//! @warning   None
//******************************************************************************
int sd_mcex_ioctl_send_psi(struct mmc_card *card, unsigned long arg)
{
   struct sd_mcex_data *data;
   int ret, status;

   data = kmalloc(sizeof(struct sd_mcex_data), GFP_KERNEL);
   if (!data)   
      return -ENOMEM;
   
   if (copy_from_user(data, (void __user *)arg, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   mmc_claim_host(card->host);
   
#ifdef DEBUG
   printk(KERN_DEBUG "McEX : send command SET_BLOCKLEN (arg=%08X)\n", 32);
#endif // DEBUG
   sd_mcex_send_command(card, MMC_SET_BLOCKLEN, 32, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC, &status);
   
#ifdef DEBUG
   printk(KERN_DEBUG "McEX : send command SEND_PSI (arg=%08X)\n", data->arg);
#endif // DEBUG
   data->error = sd_mcex_send_command_with_data(card, SD_MCEX_SEND_PSI, data->arg, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC, &status, DIR_READ, data->buffer, 32);
   
#ifdef DEBUG
   printk(KERN_DEBUG "McEX : send command SET_BLOCKLEN (arg=%08X)\n", 512);
#endif // DEBUG
   sd_mcex_send_command(card, MMC_SET_BLOCKLEN, 512, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC, &status);

   sd_mcex_wait_data_ready(card);
   mmc_release_host(card->host);

   if (copy_to_user((void __user *)arg, data, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   ret = 0;   

out:   
   kfree(data);
   return ret;
}


//******************************************************************************
//! @brief     This function implements the SD_MCEX_IOCTL_WRITE_SEC_CMD operation.
//!
//! @param[in] card the mmc_card handle
//! @param[in] arg the pointer to a sd_mcex_data structure in user space
//!            @li ((struct sd_mcex_data *)arg)->arg = unused
//!            @li ((struct sd_mcex_data *)arg)->buffer = 512-byte secure token C-APDU (in)
//!            @li ((struct sd_mcex_data *)arg)->error = command error (out)
//!
//! @return    0 in case of success, error code otherwise
//!
//! @warning   None
//******************************************************************************
int sd_mcex_ioctl_write_sec_cmd(struct mmc_card *card, unsigned long arg)
{
   struct sd_mcex_data *data;
   int ret, status;

   data = kmalloc(sizeof(struct sd_mcex_data), GFP_KERNEL);
   if (!data)   
      return -ENOMEM;
   
   if (copy_from_user(data, (void __user *)arg, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   mmc_claim_host(card->host);

#ifdef DEBUG
   printk(KERN_DEBUG "McEX : send command WRITE_SEC_CMD\n");
#endif // DEBUG
   data->error = sd_mcex_send_command_with_data(card, SD_MCEX_WRITE_SEC_CMD, 1, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC, &status, DIR_WRITE, data->buffer, 512);

   sd_mcex_wait_data_ready(card);
   mmc_release_host(card->host);

   if (copy_to_user((void __user *)arg, data, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   ret = 0;   

out:   
   kfree(data);
   return ret;
}


//******************************************************************************
//! @brief     This function implements the SD_MCEX_IOCTL_READ_SEC_CMD operation.
//!
//! @param[in] card the mmc_card handle
//! @param[in] arg the pointer to a sd_mcex_data structure in user space
//!            @li ((struct sd_mcex_data *)arg)->arg = unused
//!            @li ((struct sd_mcex_data *)arg)->buffer = 512-byte secure token R-APDU (out)
//!            @li ((struct sd_mcex_data *)arg)->error = command error (out)
//!
//! @return    0 in case of success, error code otherwise
//!
//! @warning   None
//******************************************************************************
int sd_mcex_ioctl_read_sec_cmd(struct mmc_card *card, unsigned long arg)
{
   struct sd_mcex_data *data;
   int ret, status;

   data = kmalloc(sizeof(struct sd_mcex_data), GFP_KERNEL);
   if (!data)   
      return -ENOMEM;
   
   if (copy_from_user(data, (void __user *)arg, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   mmc_claim_host(card->host);
   
#ifdef DEBUG
   printk(KERN_DEBUG "McEX : send command READ_SEC_CMD\n");
#endif // DEBUG
   data->error = sd_mcex_send_command_with_data(card, SD_MCEX_READ_SEC_CMD, 1, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC, &status, DIR_READ, data->buffer, 512);

   sd_mcex_wait_data_ready(card);
   mmc_release_host(card->host);

   if (copy_to_user((void __user *)arg, data, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   ret = 0;   

out:   
   kfree(data);
   return ret;
}


//******************************************************************************
//! @brief     This function implements the SD_MCEX_IOCTL_WRITE_BLOCK operation.
//!
//! @param[in] card the mmc_card handle
//! @param[in] arg the pointer to a sd_mcex_data structure in user space
//!            @li ((struct sd_mcex_data *)arg)->arg = address (in)
//!            @li ((struct sd_mcex_data *)arg)->buffer = 512-byte block value (in)
//!            @li ((struct sd_mcex_data *)arg)->error = command error (out)
//!
//! @return    0 in case of success, error code otherwise
//!
//! @warning   This operation is not part of the McEX specification, but is for
//!            debug purpose only.
//******************************************************************************
int sd_mcex_ioctl_write_block(struct mmc_card *card, unsigned long arg)
{
   struct sd_mcex_data *data;
   int ret, status;

   data = kmalloc(sizeof(struct sd_mcex_data), GFP_KERNEL);
   if (!data)   
      return -ENOMEM;
   
   if (copy_from_user(data, (void __user *)arg, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   mmc_claim_host(card->host);

#ifdef DEBUG
   printk(KERN_DEBUG "McEX : send command WRITE_BLOCK (arg=%08X)\n", data->arg);
#endif // DEBUG
   data->error = sd_mcex_send_command_with_data(card, MMC_WRITE_BLOCK, data->arg, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC, &status, DIR_WRITE, data->buffer, 512);

   sd_mcex_wait_data_ready(card);
   mmc_release_host(card->host);

   if (copy_to_user((void __user *)arg, data, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   ret = 0;   

out:   
   kfree(data);
   return ret;
}


//******************************************************************************
//! @brief     This function implements the SD_MCEX_IOCTL_READ_BLOCK operation.
//!
//! @param[in] card the mmc_card handle
//! @param[in] arg the pointer to a sd_mcex_data structure in user space
//!            @li ((struct sd_mcex_data *)arg)->arg = address (in)
//!            @li ((struct sd_mcex_data *)arg)->buffer = 512-byte block value (out)
//!            @li ((struct sd_mcex_data *)arg)->error = command error (out)
//!
//! @return    0 in case of success, error code otherwise
//!
//! @warning   This operation is not part of the McEX specification, but is for
//!            debug purpose only.
//******************************************************************************
int sd_mcex_ioctl_read_block(struct mmc_card *card, unsigned long arg)
{
   struct sd_mcex_data *data;
   int ret, status;

   data = kmalloc(sizeof(struct sd_mcex_data), GFP_KERNEL);
   if (!data)   
      return -ENOMEM;
   
   if (copy_from_user(data, (void __user *)arg, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   mmc_claim_host(card->host);

#ifdef DEBUG
   printk(KERN_DEBUG "McEX : send command READ_SINGLE_BLOCK (arg=%08X)\n", data->arg);
#endif // DEBUG
   data->error = sd_mcex_send_command_with_data(card, MMC_READ_SINGLE_BLOCK, data->arg, MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC, &status, DIR_READ, data->buffer, 512);

   sd_mcex_wait_data_ready(card);
   mmc_release_host(card->host);

   if (copy_to_user((void __user *)arg, data, sizeof(struct sd_mcex_data))) {
      ret = -EFAULT;
      goto out;
   }

   ret = 0;   

out:   
   kfree(data);
   return ret;
}


//******************************************************************************
//! @brief     This function implements the McEX-related ioctl operations for SD 
//!            block devices.
//!
//! @param[in] card the mmc_card handle
//! @param[in] cmd the ioctl command (i.e. one of the SD_MCEX_IOCTL_XXX constants)
//! @param[in] arg the ioctl argument (i.e. a pointer to a sd_mcex_data structure in user space)
//!
//! @return    0 in case of success, error code otherwise
//!
//! @warning   None
//******************************************************************************
int sd_mcex_ioctl(struct mmc_card *card, unsigned int cmd, unsigned long arg)
{
#ifdef DEBUG
   printk(KERN_DEBUG "McEX : process ioctl operation - cmd = %08X\n", cmd);
#endif // DEBUG

   if (!card) {
      printk(KERN_DEBUG "McEX : invalid mmc_card structure\n");
      return -ENOMEM;
   }

   switch (cmd)
   {
      case SD_MCEX_IOCTL_SWITCH:
         return sd_mcex_ioctl_switch(card, arg);

      case SD_MCEX_IOCTL_CONTROL_TRM:
         return sd_mcex_ioctl_control_trm(card, arg);

      case SD_MCEX_IOCTL_SEND_PSI:
         return sd_mcex_ioctl_send_psi(card, arg);

      case SD_MCEX_IOCTL_WRITE_SEC_CMD:
         return sd_mcex_ioctl_write_sec_cmd(card, arg);

      case SD_MCEX_IOCTL_READ_SEC_CMD:
         return sd_mcex_ioctl_read_sec_cmd(card, arg);

      case SD_MCEX_IOCTL_WRITE_BLOCK:
         return sd_mcex_ioctl_write_block(card, arg);

      case SD_MCEX_IOCTL_READ_BLOCK:
         return sd_mcex_ioctl_read_block(card, arg);

      default:
         break;
   }

   printk(KERN_DEBUG "McEX : invalid ioctl command\n");
   return -ENOTTY;
}


//******************************************************************************
// End of file
//******************************************************************************
