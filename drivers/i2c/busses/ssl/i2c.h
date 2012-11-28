/**
@file		i2c.h
@author		luanxu@solomon-systech.com
@version	1.0
@todo	
*/

#ifndef _I2C_LUANXU_
#define _I2C_LUANXU_


/** i2c API returns */
typedef enum
{
	I2C_ERR_NONE	= 0,	/**< sucessful */
	I2C_ERR_HW		= -1,	/**< hardware error */
	I2C_ERR_CMD		= -2,	/**< command error */
	I2C_ERR_PARM	= -3,	/**< parameter error */
	I2C_ERR_TOUT	= -4	/**< timeout */
}
i2c_err;


/** i2c config  */
typedef struct
{
	int		i2c_clk_hz;		/**< i2c clock freq in Hz */
	int		per_clk_hz;		/**< peripheral clock freq in Hz */
	uint8_t	addr;			/**< slave address */
	int		ack : 1;		/**< check acknowledge? */
	int		ackhi : 1;		/**< acknowledge is high polarity? */
}
i2c_cfg_t, *i2c_cfg_p;


#define I2C_CMD_MID		3	/**< continue command */
#define I2C_CMD_END		2	/**< end command only */
#define I2C_CMD_START	1	/**< start command only */
#define I2C_CMD_DONE	0	/**< complete start and end commands */


/** transfer descriptor */
typedef struct
{
	uint8_t		*buf;		/**< data buffer */
	int			len;		/**< buffer length to transfer */
	int			actual;		/**< actual length transfered */
}
i2c_td_t, *i2c_td_p;


/** cmd descriptor */
typedef struct
{
	i2c_td_t	wr;			/**< write data descriptor */
	i2c_td_t	rd;			/**< read data descriptor */
	int			cmd;		/**< start, cont, stop command */
	int			rt;			/**< return */
}
i2c_cmd_t, *i2c_cmd_p;


/** i2c context */
typedef struct
{
	/** public - initialized by client */
	void		*r;		/**< logical register base address */
	void		*ctx;	/**< context for event calllback */
	void		(*evt)(void *ctx);	/**< event callback */

	/** private - zero-d by client on init, opaque after that*/
	i2c_cmd_p	cmd;		/**< current cmd */
	int			flag;		/**< ack */
}
i2c_t, *i2c_p;


/* functions API*/

i2c_err i2c_init(i2c_p t);
/**<
initialize I2C
@param[in] t		context
@return				i2c errors
*/


void i2c_exit(i2c_p t);
/**<
disable i2c device
@param[in] t		context
*/


i2c_err i2c_cfg(i2c_p t, i2c_cfg_p cfg);
/**<
config the I2C device
@param[in] t		context
@param[in] cfg		config parameters
*/


int i2c_isr(i2c_p t);
/**<
interrupt service routine
@param[in] t		context
@return			0=normal, others=error
*/


i2c_err i2c_rw(i2c_p t, i2c_cmd_p cmd);
/**<
write and/or read buffer data to/from i2c device
@param[in] t		context
@param[in] cmd		command
@return		I2C_ERR_NONE = ok, I2C_ERR_XXX = error
}
*/


#endif
