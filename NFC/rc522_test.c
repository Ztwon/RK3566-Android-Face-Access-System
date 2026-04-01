#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <limits.h>
#include <asm/ioctls.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include "rc522.h"

#define  MAXRLEN 18 

int fd = 0;//文件句柄

void print_data(const char *title, char *dat, int count)
{
   int i = 0; 

   printf(title);

   for(i = 0; i < count; i++) 
   {
      printf(" 0x%x", dat[i]);
   }
   printf("\n");
}

unsigned char ReadRawRC(unsigned char Address)
{
  unsigned char buf[1]; 

  buf[0] = Address;
   read(fd,buf,1);  
  return buf[0];  
}

void WriteRawRC(unsigned char Address, unsigned char Value)
{  
  unsigned char buf[2];

  buf[0] = Address;
  buf[1] = Value;
  write(fd,buf,2);  
}

void SetBitMask(unsigned char ucReg, unsigned char ucMask)  
{
  unsigned char ucTemp;

  ucTemp = ReadRawRC ( ucReg );
  WriteRawRC (ucReg, ucTemp | ucMask); // set bit mask
}

void ClearBitMask(unsigned char ucReg, unsigned char ucMask)  
{
  unsigned char ucTemp;

  ucTemp = ReadRawRC (ucReg);
  WriteRawRC (ucReg, ucTemp & ( ~ ucMask)); // clear bit mask
}

void PcdAntennaOn(void)
{
  unsigned char uc;

  uc = ReadRawRC (TxControlReg);
  if (! ( uc & 0x03 ))
  {
      SetBitMask(TxControlReg, 0x03);   
  }
}

static void PcdAntennaOff(void)
{
    ClearBitMask(TxControlReg, 0x03);
}

int PcdReset(void)
{
  fd = open("/dev/nfc", O_RDWR);

    if(fd < 0)
    {
        printf("open rc522_drv error %d\n",fd);
      return fd;
  }

  WriteRawRC ( CommandReg, 0x0f );
  
  while ( ReadRawRC ( CommandReg ) & 0x10 );
 
   //定义发送和接收常用模式 和Mifare卡通讯，CRC初始值0x6363
   WriteRawRC ( ModeReg, 0x3D );          
   WriteRawRC ( TReloadRegL, 30 );      //16位定时器低位   
   WriteRawRC ( TReloadRegH, 0 );      //16位定时器高位 
   WriteRawRC ( TModeReg, 0x8D );      //定义内部定时器的设置 
   WriteRawRC ( TPrescalerReg, 0x3E );   //设置定时器分频系数  
   WriteRawRC ( TxAutoReg, 0x40 );       //调制发送信号为100%ASK 
  return 1;
}

char M500PcdConfigISOType ( unsigned char ucType )
{
  if ( ucType == 'A')                     //ISO14443_A
   {
      ClearBitMask ( Status2Reg, 0x08 );    
      WriteRawRC ( ModeReg, 0x3D );         //3F    
      WriteRawRC ( RxSelReg, 0x86 );        //84    
      WriteRawRC( RFCfgReg, 0x7F );         //4F    
      WriteRawRC( TReloadRegL, 30 );            
      WriteRawRC ( TReloadRegH, 0 );    
      WriteRawRC ( TModeReg, 0x8D );    
      WriteRawRC ( TPrescalerReg, 0x3E );   
      usleep(10000);    
      PcdAntennaOn ();//开天线   
   } 
   else
   {
      return MI_ERR;
   }
   return MI_OK;   
}

char PcdComMF522 ( unsigned char ucCommand, unsigned char * pInData, unsigned char ucInLenByte, unsigned char * pOutData,unsigned int * pOutLenBit )    
{
  char cStatus = MI_ERR;
  unsigned char ucIrqEn   = 0x00;
  unsigned char ucWaitFor = 0x00;
  unsigned char ucLastBits;
  unsigned char ucN;
  unsigned int ul;

  switch ( ucCommand )
  {
     case PCD_AUTHENT:      //Mifare认证
        ucIrqEn   = 0x12;   //允许错误中断请求ErrIEn  允许空闲中断IdleIEn
        ucWaitFor = 0x10;   //认证寻卡等待时候 查询空闲中断标志位
        break;
     
     case PCD_TRANSCEIVE:   //接收发送 发送接收
        ucIrqEn   = 0x77;   //允许TxIEn RxIEn IdleIEn LoAlertIEn ErrIEn TimerIEn
        ucWaitFor = 0x30;   //寻卡等待时候 查询接收中断标志位与 空闲中断标志位
        break;
     
     default:
       break;     
  }
  //IRqInv置位管脚IRQ与Status1Reg的IRq位的值相反 
  WriteRawRC ( ComIEnReg, ucIrqEn | 0x80 );
  //Set1该位清零时，CommIRqReg的屏蔽位清零
  ClearBitMask ( ComIrqReg, 0x80 );  
  //写空闲命令
  WriteRawRC ( CommandReg, PCD_IDLE );     
  
  //置位FlushBuffer清除内部FIFO的读和写指针以及ErrReg的BufferOvfl标志位被清除
  SetBitMask ( FIFOLevelReg, 0x80 );      

  for ( ul = 0; ul < ucInLenByte; ul ++ )
  {
      WriteRawRC ( FIFODataReg, pInData [ ul ] ); //写数据进FIFOdata
  }
    
  WriteRawRC ( CommandReg, ucCommand );         //写命令

  if ( ucCommand == PCD_TRANSCEIVE )
  {  
      //StartSend置位启动数据发送 该位与收发命令使用时才有效
      SetBitMask(BitFramingReg,0x80);           
  }

  ul = 1000;                             //根据时钟频率调整，操作M1卡最大等待时间25ms

  do                                     //认证 与寻卡等待时间 
  {
       ucN = ReadRawRC ( ComIrqReg );    //查询事件中断
       ul --;
  } while ( ( ul != 0 ) && ( ! ( ucN & 0x01 ) ) && ( ! ( ucN & ucWaitFor ) ) ); 

  ClearBitMask ( BitFramingReg, 0x80 );  //清理允许StartSend位

  if ( ul != 0 )
  {
    //读错误标志寄存器BufferOfI CollErr ParityErr ProtocolErr
    if ( ! ( ReadRawRC ( ErrorReg ) & 0x1B ) )  
    {
      cStatus = MI_OK;
      
      if ( ucN & ucIrqEn & 0x01 )       //是否发生定时器中断
        cStatus = MI_NOTAGERR;   
        
      if ( ucCommand == PCD_TRANSCEIVE )
      {
        //读FIFO中保存的字节数
        ucN = ReadRawRC ( FIFOLevelReg );             
        
        //最后接收到得字节的有效位数
        ucLastBits = ReadRawRC ( ControlReg ) & 0x07; 
        
        if ( ucLastBits )
          
          //N个字节数减去1（最后一个字节）+最后一位的位数 读取到的数据总位数
          * pOutLenBit = ( ucN - 1 ) * 8 + ucLastBits;    
        else
          * pOutLenBit = ucN * 8;      //最后接收到的字节整个字节有效
        
        if ( ucN == 0 )   
          ucN = 1;    
        
        if ( ucN > MAXRLEN )
          ucN = MAXRLEN;   
        
        for ( ul = 0; ul < ucN; ul ++ )
          pOutData [ ul ] = ReadRawRC ( FIFODataReg );   
        
        }        
    }   
    else
      cStatus = MI_ERR;       
  }

  SetBitMask ( ControlReg, 0x80 );           // stop timer now
  WriteRawRC ( CommandReg, PCD_IDLE ); 
   
  return cStatus;
}

char PcdRequest (unsigned char ucReq_code, unsigned char * pTagType)
{
  char cStatus;  
  unsigned char ucComMF522Buf [ MAXRLEN ]; 
  unsigned int ulLen;

  //清理指示MIFARECyptol单元接通以及所有卡的数据通信被加密的情况
  ClearBitMask ( Status2Reg, 0x08 );
  //发送的最后一个字节的 七位
  WriteRawRC ( BitFramingReg, 0x07 );

  //ClearBitMask ( TxControlReg, 0x03 );  
  //TX1,TX2管脚的输出信号传递经发送调制的13.56的能量载波信号
  //usleep(10000); 
  SetBitMask ( TxControlReg, 0x03 );  

  ucComMF522Buf [ 0 ] = ucReq_code;   //存入 卡片命令字

  cStatus = PcdComMF522 ( PCD_TRANSCEIVE, ucComMF522Buf, 1, ucComMF522Buf,& ulLen );  //寻卡  

  if ( ( cStatus == MI_OK ) && ( ulLen == 0x10 ) )  //寻卡成功返回卡类型 
  {    
     * pTagType = ucComMF522Buf[0];
     * ( pTagType + 1 ) = ucComMF522Buf[1];
  }
  else
  {
      cStatus = MI_ERR;
  }

  return cStatus;  
}

char PcdAnticoll ( unsigned char * pSnr )
{
  char cStatus;
  unsigned char uc, ucSnr_check = 0;
  unsigned char ucComMF522Buf [ MAXRLEN ]; 
  unsigned int ulLen;
  
  //清MFCryptol On位 只有成功执行MFAuthent命令后，该位才能置位
  ClearBitMask ( Status2Reg, 0x08 );
  //清理寄存器 停止收发
  WriteRawRC ( BitFramingReg, 0x00);  
  //清ValuesAfterColl所有接收的位在冲突后被清除
  ClearBitMask ( CollReg, 0x80 );       
 
  ucComMF522Buf [ 0 ] = 0x93;           //卡片防冲突命令
  ucComMF522Buf [ 1 ] = 0x20;
 
  cStatus = PcdComMF522 ( PCD_TRANSCEIVE, 
                          ucComMF522Buf,
                          2, 
                          ucComMF522Buf,
                          & ulLen);      //与卡片通信

  if ( cStatus == MI_OK)                //通信成功
  {
    for ( uc = 0; uc < 4; uc ++ )
    {
       * ( pSnr + uc )  = ucComMF522Buf [ uc ]; //读出UID
       ucSnr_check ^= ucComMF522Buf [ uc ];
    }
    
    if ( ucSnr_check != ucComMF522Buf [ uc ] )
      cStatus = MI_ERR;            
  }
  
  SetBitMask ( CollReg, 0x80 );
      
  return cStatus;   
}

void CalulateCRC ( unsigned char * pIndata, unsigned char ucLen, unsigned char * pOutData )
{
  unsigned char uc, ucN;

  ClearBitMask(DivIrqReg,0x04);

  WriteRawRC(CommandReg,PCD_IDLE);

  SetBitMask(FIFOLevelReg,0x80);

  for ( uc = 0; uc < ucLen; uc ++)
    WriteRawRC ( FIFODataReg, * ( pIndata + uc ) );   

  WriteRawRC ( CommandReg, PCD_CALCCRC );

  uc = 0xFF;

  do 
  {
      ucN = ReadRawRC ( DivIrqReg );
      uc --;
  } while ( ( uc != 0 ) && ! ( ucN & 0x04 ) );
  
  pOutData [ 0 ] = ReadRawRC ( CRCResultRegL );
  pOutData [ 1 ] = ReadRawRC ( CRCResultRegM );   
}

char PcdSelect ( unsigned char * pSnr )
{
  char ucN;
  unsigned char uc;
  unsigned char ucComMF522Buf [ MAXRLEN ]; 
  unsigned int  ulLen;
  
  
  ucComMF522Buf [ 0 ] = PICC_ANTICOLL1;
  ucComMF522Buf [ 1 ] = 0x70;
  ucComMF522Buf [ 6 ] = 0;

  for ( uc = 0; uc < 4; uc ++ )
  {
    ucComMF522Buf [ uc + 2 ] = * ( pSnr + uc );
    ucComMF522Buf [ 6 ] ^= * ( pSnr + uc );
  }
  
  CalulateCRC ( ucComMF522Buf, 7, & ucComMF522Buf [ 7 ] );

  ClearBitMask ( Status2Reg, 0x08 );

  ucN = PcdComMF522 ( PCD_TRANSCEIVE,
                     ucComMF522Buf,
                     9,
                     ucComMF522Buf, 
                     & ulLen );
  
  if ( ( ucN == MI_OK ) && ( ulLen == 0x18 ) )
    ucN = MI_OK;  
  else
    ucN = MI_ERR;    
  
  return ucN;   
}

char PcdAuthState ( unsigned char ucAuth_mode, unsigned char ucAddr, unsigned char * pKey, unsigned char * pSnr )
{
  char cStatus;
  unsigned char uc, ucComMF522Buf [ MAXRLEN ];
  unsigned int ulLen;
  
  ucComMF522Buf [ 0 ] = ucAuth_mode;
  ucComMF522Buf [ 1 ] = ucAddr;

  for ( uc = 0; uc < 6; uc ++ )
    ucComMF522Buf [ uc + 2 ] = * ( pKey + uc );   

  for ( uc = 0; uc < 6; uc ++ )
    ucComMF522Buf [ uc + 8 ] = * ( pSnr + uc );   

  cStatus = PcdComMF522 ( PCD_AUTHENT,
                          ucComMF522Buf, 
                          12,
                          ucComMF522Buf,
                          & ulLen );

  if ( ( cStatus != MI_OK ) || ( ! ( ReadRawRC ( Status2Reg ) & 0x08 ) ) )
    cStatus = MI_ERR;   
    
  return cStatus;
}

char PcdWrite ( unsigned char ucAddr, unsigned char * pData )
{
  char cStatus;
  unsigned char uc, ucComMF522Buf [ MAXRLEN ];
  unsigned int ulLen;
   
  
  ucComMF522Buf [ 0 ] = PICC_WRITE;
  ucComMF522Buf [ 1 ] = ucAddr;

  CalulateCRC ( ucComMF522Buf, 2, & ucComMF522Buf [ 2 ] );

  cStatus = PcdComMF522 ( PCD_TRANSCEIVE,
                          ucComMF522Buf,
                          4, 
                          ucComMF522Buf,
                          & ulLen );

  if ( ( cStatus != MI_OK ) || ( ulLen != 4 ) || 
         ( ( ucComMF522Buf [ 0 ] & 0x0F ) != 0x0A ) )
    cStatus = MI_ERR;   
      
  if ( cStatus == MI_OK )
  {
    //memcpy(ucComMF522Buf, pData, 16);
    for ( uc = 0; uc < 16; uc ++ )
      ucComMF522Buf [ uc ] = * ( pData + uc );  
    
    CalulateCRC ( ucComMF522Buf, 16, & ucComMF522Buf [ 16 ] );

    cStatus = PcdComMF522 ( PCD_TRANSCEIVE,
                           ucComMF522Buf, 
                           18, 
                           ucComMF522Buf,
                           & ulLen );
    
    if ( ( cStatus != MI_OK ) || ( ulLen != 4 ) || 
         ( ( ucComMF522Buf [ 0 ] & 0x0F ) != 0x0A ) )
      cStatus = MI_ERR;   
    
  }   
  return cStatus;   
}

char PcdRead ( unsigned char ucAddr, unsigned char * pData )
{
  char cStatus;
  unsigned char uc, ucComMF522Buf [ MAXRLEN ]; 
  unsigned int ulLen;
  
  ucComMF522Buf [ 0 ] = PICC_READ;
  ucComMF522Buf [ 1 ] = ucAddr;

  CalulateCRC ( ucComMF522Buf, 2, & ucComMF522Buf [ 2 ] );
 
  cStatus = PcdComMF522 ( PCD_TRANSCEIVE,
                          ucComMF522Buf,
                          4, 
                          ucComMF522Buf,
                          & ulLen );

  if ( ( cStatus == MI_OK ) && ( ulLen == 0x90 ) )
  {
    for ( uc = 0; uc < 16; uc ++ )
      * ( pData + uc ) = ucComMF522Buf [ uc ];   
  }
  
  else
    cStatus = MI_ERR;   
   
  return cStatus;   
}

char PcdHalt( void )
{
  unsigned char ucComMF522Buf [ MAXRLEN ]; 
  unsigned int  ulLen;
  

  ucComMF522Buf [ 0 ] = PICC_HALT;
  ucComMF522Buf [ 1 ] = 0;
  
   CalulateCRC ( ucComMF522Buf, 2, & ucComMF522Buf [ 2 ] );
  PcdComMF522 ( PCD_TRANSCEIVE,
                ucComMF522Buf,
                4, 
                ucComMF522Buf, 
                & ulLen );

  return MI_OK; 
}

int open_door()
{
		int fd_pwm = open("/proc/door/state", O_WRONLY); 
		if (fd_pwm == -1) {
			perror("Failed to open /proc/door/state for writing");
			return EXIT_FAILURE;
		}
		
		const char *value_to_write = "1";
		if (write(fd_pwm, value_to_write, 1) == -1) {
			perror("Failed to write to /proc/door/state");
			close(fd_pwm);
			return EXIT_FAILURE;
		}	
		
		close(fd_pwm);	
}

int main(int argc, const char * argv [ ])
{
  int ret = -1;
  unsigned char KeyValue[]={0xFF ,0xFF, 0xFF, 0xFF, 0xFF, 0xFF};   // 卡A密钥
  char cStr[30], writebuf[16], readbuf[16];
  unsigned char ucArray_ID[4];    /*先后存放IC卡的类型和UID(IC卡序列号)*/                                                                                         
  unsigned char ucStatusReturn, snr;      /*返回状态*/     
  snr = 1; //扇区号
  
  size_t i;
  
  const char *strArray[] = {
	  "D4848E02",
	  "96760201",
	  "7A8B9C0D",
    "476DCD05",
    "1E54A004"
  };
  
  size_t arraySize = sizeof(strArray) / sizeof(strArray[0]);

  ret = PcdReset();
  if(ret < 0)
  {
        printf("rc522 rst error %d \n",ret);
      return 0;
  }
  ucStatusReturn = M500PcdConfigISOType ( 'A' );
   if(ucStatusReturn == MI_ERR)
   {
      printf("M500PcdConfigISOType error! \n");
   }
   else
   {
      printf("M500PcdConfigISOType normal! \n");
   }

   while(1)
   {
         ucStatusReturn = PcdRequest (PICC_REQIDL, ucArray_ID); 
         if (ucStatusReturn == MI_OK)  
         {
            if (PcdAnticoll(ucArray_ID) == MI_OK)  
            {
                  PcdSelect(ucArray_ID);      // 选卡 
                  ucStatusReturn = PcdAuthState(KEYA, (snr*4 + 3) , KeyValue, ucArray_ID );//校验密码 
                  if(ucStatusReturn !=  MI_OK)
                  {
                    sprintf(cStr, "%02X%02X%02X%02X",ucArray_ID[0], ucArray_ID[1], ucArray_ID[2],ucArray_ID[3]);
                    printf("The Card ID is: %s \r\n",cStr);  
					 
					for (i = 0; i < arraySize; i++) {
						if (strcmp(strArray[i], cStr) == 0) {
							printf("Registered id, open the door\n");
							open_door();
						}
					}		
					sleep(1);					
                  }
                  else
                  {
                        printf("rc522 card number err!\r\n");
                  }


            } 
            else
            {
                  PcdHalt();
            }                                       
         }
   }

   return 0; 
}
