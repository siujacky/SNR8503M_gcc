/*******************************************************************************
 * ��Ȩ���� (C)2015, SNANER SEMICONDUCTOR Co.ltd
 *
 * �ļ����ƣ� hardware_init.c
 * �ļ���ʶ��
 * ����ժҪ�� Ӳ����ʼ������
 * ����˵���� ��
 * ��ǰ�汾�� V 1.0
 * ��    �ߣ� Li 
 * ������ڣ� 2021��11��15��
 *
 *******************************************************************************/
#include "hardware_config.h"
#include "hardware_init.h"
#include "basic.h"
#include "MC_Parameter.h"
#include "feature_config.h"
#include "UR_Ctrl.h"
#include "main.h"

void Multiplex_nRST(void);
void Multiplex_SWD(void);
/*******************************************************************************
 �������ƣ�    void Hardware_init(void)
 ����������    Ӳ�����ֳ�ʼ��
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li          ����
 *******************************************************************************/
void Hardware_init(void)
{
    __disable_irq();                  /* �ر��ж� �ж��ܿ��� */
		SYS_WR_PROTECT = 0x7a83;          /* ʹ��ϵͳ�Ĵ���д���� */  
    IWDG_DISABLE();                   /* �رն������Ź�ʹ��*/
    FLASH_CFG |= 0x00080000;          /* FLASH Ԥȡ����ʹ��*/	
    delay_init(48);                   /* ��ʱ������ʼ����Ƶ48MHz*/
		delay_ms(100);
	
		#if (UART0_FUNCTION == ENABLE_FUNCTION)
			UART0_init();										/* ���ڳ�ʼ��UART0*/
		#endif
	
    GPIO_init();                      /* GPIO��ʼ�� */	
		HALL_Perip_Init(CLK_DIVISION);  	/* hall��ʼ��*/
		PGA_init();                       /* PGA ��ʼ�� */
	  ADC_init();                       /* ADC��ʼ�� */
	  DAC_init();                       /* DAC ��ʼ�� */
	  CMP_init();                       /* �Ƚ�����ʼ�� */	  
    UTimer_init(CLK_DIVISION);        /* UTimer��ʼ�� */
    MCPWM_init();                     /* MCPWM��ʼ�� */
#if MODULE_MOTOR_HALL
		BLDC_init();											/* Hall BLDC init from closed lib */
#endif
#if MODULE_MOTOR_SENSORLESS
		{ extern void BLDC_init_snls(void); BLDC_init_snls(); }   /* Sensorless BLDC init from renamed lib */
#endif
	
		DSP_Cmd(ENABLE);
	
		SoftDelay(500000);                /* ��ʱ�ȴ�Ӳ����ʼ���ȶ� 1000--134us*/

		NVIC_SetPriority(HALL_IRQn, 0);		/* ����HALL�ж����ȼ� */
		NVIC_EnableIRQ(HALL_IRQn);     		/* ʹ��HALL�ж� */
				
    NVIC_SetPriority(ADC_IRQn, 1);    /* ADC�ж����ȼ�����*/
    NVIC_EnableIRQ(ADC_IRQn);         /* ʹ��ADC�ж�*/	
	
    NVIC_SetPriority(TIMER0_IRQn, 2); /* TIMER0�ж����ȼ�����*/
    NVIC_EnableIRQ(TIMER0_IRQn);      /* ʹ��TIMER0�ж�*/	

    NVIC_SetPriority(TIMER1_IRQn, 0); /* TIMER1�ж����ȼ�����*/
    NVIC_EnableIRQ(TIMER1_IRQn);      /* ʹ��TIMER1�ж�*/		

    NVIC_SetPriority(MCPWM0_IRQn, 2); /* MCPWM0�ж����ȼ�����*/
    NVIC_EnableIRQ(MCPWM0_IRQn);      /* ʹ��MCPWM0�ж�*/		

    NVIC_SetPriority(CMP_IRQn, 0);    /* CMP�ж����ȼ�����*/	
		NVIC_EnableIRQ(CMP_IRQn);         /* ʹ��CMP�ж�*/	

		#if (UART0_FUNCTION == ENABLE_FUNCTION)
    NVIC_SetPriority(UART_IRQn, 1);   /* UART_IRQn�ⲿ�ж����ȼ�����Ϊ1*/
    NVIC_EnableIRQ(UART_IRQn);        /* ʹ��UART_IRQn�ⲿ�ж�*/
		#endif

    delay_us(100);                    /* ��ʱ�ȴ�Ӳ����ʼ���ȶ� */
    __enable_irq();                   /* �������ж� */
}

/*******************************************************************************
 �������ƣ�    void Clock_Init(void)  
 ����������    ʱ������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li          ����
 *******************************************************************************/
void Clock_Init(void)
{
	  delay_init(96);            /* ��ʱ������ʼ����Ƶ96MHz*/
    SYS_WR_PROTECT = 0x7a83;   /* ���ϵͳ�Ĵ���д���� */
    SYS_AFE_REG0 |= BIT15;     /* BIT15:PLLPDN */
    SoftDelay(100);            /* ��ʱ100us,�ȴ�PLL�ȶ�*/
    SYS_CLK_CFG |= 0x000001ff; /* BIT8:0: CLK_HS,1:PLL  | BIT[7:0]CLK_DIV  | 1ff��Ӧ96Mʱ�� */
	  SYS_SoftResetModule(0xfff);/* ��λ��������Ĵ��� */
}

/*******************************************************************************
 �������ƣ�    void SystemInit(void)
 ����������    Ӳ��ϵͳ��ʼ��������ʱ�ӳ�ʼ������
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void SystemInit(void)
{
    Clock_Init(); /* ʱ�ӳ�ʼ�� */
}

/*******************************************************************************
 �������ƣ�    void GPIO_init(void)
 ����������    GPIOӲ����ʼ��
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/11/15      V1.0        Li        ����
 *******************************************************************************/
void GPIO_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
	
    /* MCPWM P0.10~P1.15 */
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT; /*���ģʽ*/
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIO0, &GPIO_InitStruct);
    /* P0.10~P1.15 ���ó�MCPWM���� */
	  GPIO_PinAFConfig(GPIO0, GPIO_PinSource_10, AF3_MCPWM);
    GPIO_PinAFConfig(GPIO0, GPIO_PinSource_11, AF3_MCPWM);
    GPIO_PinAFConfig(GPIO0, GPIO_PinSource_12, AF3_MCPWM);
    GPIO_PinAFConfig(GPIO0, GPIO_PinSource_13, AF3_MCPWM);
    GPIO_PinAFConfig(GPIO0, GPIO_PinSource_14, AF3_MCPWM);
    GPIO_PinAFConfig(GPIO0, GPIO_PinSource_15, AF3_MCPWM);

	  //GPIO_P0.4/P0.5/P0.6 ����Ϊ HALL_IN0/HALL_IN1 ģ��ͨ��
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;
		GPIO_Init(GPIO0, &GPIO_InitStruct);
		GPIO_PinAFConfig(GPIO0, GPIO_PinSource_4, AF2_HALL);
		GPIO_PinAFConfig(GPIO0, GPIO_PinSource_5, AF2_HALL);
		GPIO_PinAFConfig(GPIO0, GPIO_PinSource_6, AF2_HALL);
	
  	/* P1.3 FG */
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT; /*���ģʽ*/
    GPIO_InitStruct.GPIO_Pin = GPIO_PIN_FG;    
    GPIO_Init(GPIO_FG, &GPIO_InitStruct);	

			/* P0.9 CW-CCW */
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;  	/*GPIO����ģʽ*/
    GPIO_InitStruct.GPIO_Pin = GPIO_PIN_CWCCW;   
		GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP; 	
    GPIO_Init(GPIO_CWCCW, &GPIO_InitStruct);	
		
		/*P0.0 LED/NRST*/
		Multiplex_nRST();
	  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT; /*���ģʽ*/
    GPIO_InitStruct.GPIO_Pin = GPIO_PIN_LED;     
    GPIO_Init(GPIO_LED, &GPIO_InitStruct);		
		GPIO_WriteBit(GPIO_LED, GPIO_PIN_LED, Bit_SET);
}

/*******************************************************************************
 �������ƣ�    void HALL_Init(void)
 ����������    GPIOӲ����ʼ��
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2018/11/5      V1.0           Howlet Li          ����
 *******************************************************************************/
void HALL_Perip_Init(uint32_t Clk_Division)
{
  HALL_InitTypeDef HALL_InitStruct;
  HALL_StructInit(&HALL_InitStruct);
	
  HALL_InitStruct.HALL_Ena = ENABLE;             	/* ģ��ʹ�� */  
	
  HALL_InitStruct.ClockDivision = Clk_Division; 	/* ����Hallģ��ʱ�ӷ�Ƶϵ�� */
  HALL_InitStruct.CountTH = 9600000;             	/* Hallģ�����ģֵ����������ģֵ�������ʱ�ж� */
  HALL_InitStruct.FilterLen = 63;               	/* Hall�ź������˲����� 256��ʱ������ */
	
  HALL_InitStruct.Filter75_Ena = ENABLE;         	/* Hall�ź��˲���ʽ��7��5ģʽ����ȫ1��Чģʽ */
	
  HALL_InitStruct.Capture_IRQ_Ena = ENABLE;      	/* ��׽�ж�ʹ�� */
  HALL_InitStruct.OverFlow_IRQ_Ena = ENABLE;     	/* ��ʱ�ж�ʹ�� */
  HALL_InitStruct.softIE = DISABLE;              	/* �����ж�ʹ�� */
  HALL_Init(&HALL_InitStruct);
}

/*******************************************************************************
 �������ƣ�    void ADC0_init(void)
 ����������    ADC0Ӳ����ʼ��
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2015/11/5      V1.0           Li Li          ����
 *******************************************************************************/
void ADC_init(void)
{
    ADC_InitTypeDef ADC_InitStructure;

    ADC_StructInit(&ADC_InitStructure);                       /*ADC��ʼ���ṹ��*/
    ADC_InitStructure.Align = ADC_LEFT_ALIGN;                 /* ADC������������*/
    ADC_InitStructure.Trigger_Mode = ADC_1SEG_TRG;            /* ����ADCת��ģʽΪ4��ʽ���� */
    ADC_InitStructure.FirSeg_Ch = 4;                          /* ��һ�ι�����8��ͨ�� */
    ADC_InitStructure.SecSeg_Ch = 0;                          /* �ڶ��ι�����0��ͨ�� */
    ADC_InitStructure.ThrSeg_Ch = 0;                          /* �����ι�����0��ͨ�� */
    ADC_InitStructure.FouSeg_Ch = 0;                          /* ���Ķι�����0��ͨ�� */
    ADC_InitStructure.Trigger_Cnt = 0;                        /* ���δ���ģʽ�´���һ�β�������Ҫ���¼���:
																																0~15 0��ʾ��Ҫһ�δ�����15��ʾ��Ҫ16�� */
    ADC_InitStructure.Trigger_En = ADC_HARDWARE_T0_TRG;    	  /* ����T0��T1Ӳ�������¼� */
    ADC_InitStructure.SEL_En = ADC_MCPWM_SEL;                 /* MCPWM����ADC���� */
    ADC_InitStructure.IE = ADC_EOS0_IRQ_EN;                   /* ��һ��ɨ������ж�*/
    ADC_InitStructure.ADC_SAMP_CLK = 8;                      	/* ���ò���ʱ��Ϊ20��ADCʱ������ ��Χ 4--35*/
    ADC_InitStructure.IE = ADC_EOS0_IRQ_EN;                   /* ��һ�Ρ��ڶ��Ρ�����ֵɨ������ж� */
    ADC_Init(ADC, &ADC_InitStructure);										
		
    ADC_CHN0 = ADC_PEAK_CUR_CHANNEL  | (ADC_BUS_VOL_CHANNEL << 4) | ( ADC_SPEED_CHANNEL << 8) | (ADC_MOS_TEMP_CHANNEL << 12);
		
    ADC_IF = 0xff;                                            /* ���жϱ�־λ	*/
    ADC_CFG |= BIT11;                                         /* ״̬����λ�����ź�	*/
}

/*******************************************************************************
 �������ƣ�    void UTimer_init(uint32_t Clk_Division)
 ����������    UTimerӲ����ʼ��
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2015/11/5      V1.0           Li Li          ����
 *******************************************************************************/
void UTimer_init(uint32_t Clk_Division)
{
    TIM_TimerInitTypeDef TIM_InitStruct;
	
    TIM_TimerStrutInit(&TIM_InitStruct); 				/* Timer�ṹ�������ʼ�� */
    TIM_InitStruct.EN = ENABLE;                 /* ģ��ʹ�� */
    TIM_InitStruct.CH1_MODE = TIMER_OPMode_CAP; /* ����Timer CH1Ϊ����ģʽ*/
    TIM_InitStruct.CH1_FE_CAP_EN = DISABLE;    
    TIM_InitStruct.CH1_RE_CAP_EN = DISABLE;   
    TIM_InitStruct.SRC1 = TIM_SRC0_1;           /* ����ģʽͨ��1�ź���Դ,Timerͨ��1*/
    TIM_InitStruct.CAP1_CLR_EN = DISABLE;       /* ������CAP1�����¼�ʱ������Timer������*/
    TIM_InitStruct.TH = 24000;                  /* ���ü���������ģֵ */
    TIM_InitStruct.FLT = 0;                     /* ͨ�� 0/1 �ź��˲�����ѡ��0-255 */
    TIM_InitStruct.CLK_DIV = TIM_Clk_Div1;      /* ����Timerģ�����ݷ�Ƶϵ�� */
    TIM_InitStruct.IE = TIM_IRQ_IE_ZC;          /* ʹ��Timerģ��CH1�Ƚ��ж� */
    TIM_TimerInit(TIMER0, &TIM_InitStruct);

		// TIMER1 ����������(��ǰ����)����, 
    TIM_TimerStrutInit(&TIM_InitStruct);        /* Timer�ṹ�������ʼ�� */
    TIM_InitStruct.EN = DISABLE;                /* ģ��ʹ�� */
    TIM_InitStruct.CH0_MODE = TIMER_OPMode_CAP; /* ����Timer CH0Ϊ����ģʽ*/
    TIM_InitStruct.CH0_FE_CAP_EN = DISABLE;     
    TIM_InitStruct.CH0_RE_CAP_EN = DISABLE;     
    TIM_InitStruct.SRC0 = TIM_SRC1_0;           /* ����ģʽͨ��1�ź���Դ,Timerͨ��0*/
    TIM_InitStruct.CAP0_CLR_EN = DISABLE;       /* ������CAP0�����¼�ʱ������Timer������*/
    TIM_InitStruct.TH = 48000;     							/* ���ü���������ģֵ*/
    TIM_InitStruct.FLT = 0;                     /* ͨ�� 0/1 �ź��˲�����ѡ��0-255 */
    TIM_InitStruct.CLK_DIV = Clk_Division; 	//��Hallģ��ķ�Ƶ����һ��     /* ����Timerģ�����ݷ�Ƶϵ�� */
    TIM_InitStruct.IE = TIM_IRQ_IE_ZC;          /* ʹ��Timerģ��CH0�Ƚ��ж� */
    TIM_TimerInit(TIMER1, &TIM_InitStruct);
}

/*******************************************************************************
 �������ƣ�    void MCPWM_init(void)
 ����������    MCPWM_initӲ����ʼ��
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2021/8/25      V1.0           Li              ����
 *******************************************************************************/
void MCPWM_init(void)
{
    MCPWM_InitTypeDef MCPWM_InitStructure;
    MCPWM_StructInit(&MCPWM_InitStructure);

    MCPWM_InitStructure.CLK_DIV = 0;            /* MCPWMʱ�ӷ�Ƶ���� */
    MCPWM_InitStructure.MCLK_EN = ENABLE;       /* ģ��ʱ�ӿ��� */
    MCPWM_InitStructure.MCPWM_Cnt0_EN = ENABLE; /* ʱ��0����������ʼ����ʹ�ܿ��� */
    MCPWM_InitStructure.MCPWM_WorkModeCH0 =	CENTRAL_PWM_MODE;	
    MCPWM_InitStructure.MCPWM_WorkModeCH1 = CENTRAL_PWM_MODE; /* ͨ������ģʽ���ã����Ķ������ض��� */
    MCPWM_InitStructure.MCPWM_WorkModeCH2 = CENTRAL_PWM_MODE;	
	
    /* �Զ�����ʹ�ܼĴ��� MCPWM_TH00 �Զ�����ʹ�� MCPWM_TMR0 �Զ�����ʹ�� MCPWM_0TH �Զ�����ʹ�� MCPWM_0CNT �Զ�����ʹ��*/
    MCPWM_InitStructure.AUEN = TH00_AUEN | TH01_AUEN | TH10_AUEN | TH11_AUEN |
                               TH20_AUEN | TH21_AUEN | TMR0_AUEN | TMR1_AUEN |
                               TH0_AUEN;
	
    MCPWM_InitStructure.GPIO_BKIN_Filter = 12;                	/* ��ͣ�¼�(����IO���ź�)�����˲���ʱ������ */
    MCPWM_InitStructure.CMP_BKIN_Filter = 12;                 	/* ��ͣ�¼�(���ԱȽ����ź�)�����˲���ʱ������ */	
	
    MCPWM_InitStructure.TimeBase0_PERIOD = PWM_PERIOD;          /* ʱ��0�������� */
    MCPWM_InitStructure.TriggerPoint0 = (u16)( -100 ); 					//10 - PWM_PERIOD /* MCPWM_TMR0 ADC�����¼�T0 ���� */
    MCPWM_InitStructure.TriggerPoint1 = (u16)(300); 						/* MCPWM_TMR1 ADC�����¼�T1 ���� */

    MCPWM_InitStructure.DeadTimeCH0123N = DEADTIME;
    MCPWM_InitStructure.DeadTimeCH0123P = DEADTIME;             /* ����ʱ������ */

		#if ((PRE_DRIVER_POLARITY == HN_HIGH__LN_HIGH)) 
    MCPWM_InitStructure.CH0P_Polarity_INV = DISABLE;            /* CH0Pͨ������������� | ���������ȡ����� */
    MCPWM_InitStructure.CH0N_Polarity_INV = DISABLE;            /* CH0Nͨ������������� | ���������ȡ�����*/
    MCPWM_InitStructure.CH1P_Polarity_INV = DISABLE;
    MCPWM_InitStructure.CH1N_Polarity_INV = DISABLE;
    MCPWM_InitStructure.CH2P_Polarity_INV = DISABLE;
    MCPWM_InitStructure.CH2N_Polarity_INV = DISABLE;

    /* Ĭ�ϵ�ƽ���� Ĭ�ϵ�ƽ�������MCPWM_IO01��MCPWM_IO23�� BIT0��BIT1��BIT8��BIT9��BIT6��BIT14
                                                     ͨ�������ͼ��Կ��Ƶ�Ӱ�죬ֱ�ӿ���ͨ����� */
    MCPWM_InitStructure.CH0P_default_output = LOW_LEVEL;
    MCPWM_InitStructure.CH0N_default_output = LOW_LEVEL;
    MCPWM_InitStructure.CH1P_default_output = LOW_LEVEL;      /* CH1P��Ӧ�����ڿ���״̬����͵�ƽ */
    MCPWM_InitStructure.CH1N_default_output = LOW_LEVEL;     /* CH1N��Ӧ�����ڿ���״̬����ߵ�ƽ */
    MCPWM_InitStructure.CH2P_default_output = LOW_LEVEL;
    MCPWM_InitStructure.CH2N_default_output = LOW_LEVEL;	
		#else
    MCPWM_InitStructure.CH0P_Polarity_INV = DISABLE;            /* CH0Pͨ������������� | ���������ȡ����� */
    MCPWM_InitStructure.CH0N_Polarity_INV = ENABLE;            /* CH0Nͨ������������� | ���������ȡ�����*/
    MCPWM_InitStructure.CH1P_Polarity_INV = DISABLE;
    MCPWM_InitStructure.CH1N_Polarity_INV = ENABLE;
    MCPWM_InitStructure.CH2P_Polarity_INV = DISABLE;
    MCPWM_InitStructure.CH2N_Polarity_INV = ENABLE;

    /* Ĭ�ϵ�ƽ���� Ĭ�ϵ�ƽ�������MCPWM_IO01��MCPWM_IO23�� BIT0��BIT1��BIT8��BIT9��BIT6��BIT14
                                                     ͨ�������ͼ��Կ��Ƶ�Ӱ�죬ֱ�ӿ���ͨ����� */
    MCPWM_InitStructure.CH0P_default_output = LOW_LEVEL;
    MCPWM_InitStructure.CH0N_default_output = HIGH_LEVEL;
    MCPWM_InitStructure.CH1P_default_output = LOW_LEVEL;      /* CH1P��Ӧ�����ڿ���״̬����͵�ƽ */
    MCPWM_InitStructure.CH1N_default_output = HIGH_LEVEL;     /* CH1N��Ӧ�����ڿ���״̬����ߵ�ƽ */
    MCPWM_InitStructure.CH2P_default_output = LOW_LEVEL;
    MCPWM_InitStructure.CH2N_default_output = HIGH_LEVEL;	
		#endif
	
		
    MCPWM_InitStructure.Switch_CH0N_CH0P = DISABLE;           /* ͨ������ѡ������ | CH0P��CH0N�Ƿ�ѡ���źŽ��� */
    MCPWM_InitStructure.Switch_CH1N_CH1P = DISABLE;           /* ͨ������ѡ������ */
    MCPWM_InitStructure.Switch_CH2N_CH2P = DISABLE;           /* ͨ������ѡ������ */
		
    MCPWM_InitStructure.DebugMode_PWM_out = ENABLE;           /* �ڽ��Ϸ�����debug����ʱ����ͣMCU����ʱ��ѡ���PWMͨ��������������ź�
                                                                 �������Ĭ�ϵ�ƽ�������������� ENABLE:������� DISABLE:���Ĭ�ϵ�ƽ */

    MCPWM_InitStructure.MCPWM_Base0T0_UpdateEN = ENABLE;      /* MCPWM T0�¼�����ʹ�� */
    MCPWM_InitStructure.MCPWM_Base0T1_UpdateEN = DISABLE;     /* MCPWM T1�¼����½�ֹ */
		
    MCPWM_InitStructure.CNT0_T0_Update_INT_EN = DISABLE;      /* T0�����¼� �ж�ʹ�ܻ�ر� */		
	
    MCPWM_InitStructure.FAIL0_INT_EN = DISABLE;               /* FAIL0�¼� �ж�ʹ�ܻ�ر� */
    MCPWM_InitStructure.FAIL0_INPUT_EN = DISABLE;             /* FAIL0ͨ����ͣ���ܴ򿪻�ر� */
    MCPWM_InitStructure.FAIL0_Signal_Sel = FAIL_SEL_CMP;      /* FAIL0�¼��ź�ѡ�񣬱Ƚ�����IO�� */
    MCPWM_InitStructure.FAIL0_Polarity = HIGH_LEVEL_VALID;    /* FAIL0�¼�����ѡ�񣬸���Ч�����Ч */

    MCPWM_InitStructure.FAIL1_INT_EN = ENABLE;               /* FAIL1�¼� �ж�ʹ�ܻ�ر� */
    MCPWM_InitStructure.FAIL1_INPUT_EN = ENABLE;             /* FAIL1ͨ����ͣ���ܴ򿪻�ر� */
    MCPWM_InitStructure.FAIL1_Signal_Sel = FAIL_SEL_CMP;      /* FAIL1�¼��ź�ѡ�񣬱Ƚ�����IO�� */
    MCPWM_InitStructure.FAIL1_Polarity = HIGH_LEVEL_VALID;    /* FAIL1�¼�����ѡ�񣬸���Ч�����Ч */

    MCPWM_PRT = 0x0000DEAD; 																	/* enter password to unlock write protection */
		//MCPWM_SWAP = 0x67;			// �Ƿ��л�PWM Output�����
		MCPWM_PRT = 0x0000CAFE;
		
    MCPWM_Init(MCPWM0, &MCPWM_InitStructure);                 /* MCPWM0 ģ���ʼ�� */
}


/*******************************************************************************
 �������ƣ�    void DAC_Init(void)
 ����������    DAC��ʼ��
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 *******************************************************************************/
void DAC_init(void)
{
	  DAC_InitTypeDef DAC_InitStruct;
    DAC_StructInit(&DAC_InitStruct);
    DAC_InitStruct.DACOUT_EN = DISABLE; /*�ر�DACͨ��IO��P0.0�����*/

    DAC_Init(&DAC_InitStruct);
    DAC_OutputValue(SHORT_CURRENT_DAC);    
}
/*******************************************************************************
 �������ƣ�    void CMP_Init(void)
 ����������    �Ƚ�����ʼ��
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 2016/3/15      V1.0           Li Li          ����
 *******************************************************************************/
void CMP_init(void)
{
    CMP_InitTypeDef CMP_InitStruct;
    CMP_StructInit(&CMP_InitStruct);
		
    SYS_AnalogModuleClockCmd(SYS_AnalogModule_CMP0,DISABLE); 	/* �Ƚ���0���� */
    SYS_AnalogModuleClockCmd(SYS_AnalogModule_CMP1,ENABLE);  /* �Ƚ���1���� */
	
		//CMP_InitStruct.CMP_HYS = ENABLE;               /* �Ƚ����رջز�20MV */ 	  
    CMP_InitStruct.FIL_CLK10_DIV16 = 5;           /* ���˲�����=tclk ����*16*CMP_FltCnt */
    CMP_InitStruct.CLK10_EN = ENABLE;              /* ʱ��ʹ�� */
	
    CMP_InitStruct.CMP0_SELN = CMP0_SELN_DAC;
    CMP_InitStruct.CMP0_SELP = CMP0_SELP_CMP0_IP0; 		 /* CMP0_P CMP0_IP0ĸ�� */
    CMP_InitStruct.CMP0_IN_EN = DISABLE;            /* �Ƚ����ź�����ʹ�� */
    CMP_InitStruct.CMP0_IE = DISABLE;              /* �Ƚ���0�ź��ж�ʹ�� */
		
    CMP_InitStruct.CMP1_SELN = CMP1_SELN_DAC;      /* CMP1_N �ڲ�DAC ��� CMP1_SELN_DAC */
    CMP_InitStruct.CMP1_SELP = CMP1_SELP_CMP1_IP0; /* CMP1_P CMP1_IP0ĸ��*/
    CMP_InitStruct.CMP1_IN_EN = ENABLE;            /* �Ƚ����ź�����ʹ�� */
    CMP_InitStruct.CMP1_IE = ENABLE;              /* �Ƚ���1�ź��ж�ʹ�� */ 

    CMP_Init(&CMP_InitStruct);
}

/*******************************************************************************
 �������ƣ�    void PGA_Init(void)
 ����������    PGA��ʼ��
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 *******************************************************************************/
void PGA_init(void)
{
    SYS_AnalogModuleClockCmd(SYS_AnalogModule_OPA, ENABLE);		/* OPAʹ�� */
	
    SYS_WR_PROTECT = 0x7A83; 

		SYS_AFE_REG0 |= OPA_GIAN; 			/* OPA�������� */
	
	  SYS_WR_PROTECT = 0;
}

/*******************************************************************************
 �������ƣ�    void HALL_Init(void)
 ����������    GPIOӲ����ʼ��
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 *******************************************************************************/
void HALL_init(void)
{
    HALL_InitTypeDef HALL_InitStruct;
    HALL_StructInit(&HALL_InitStruct);
	
    HALL_InitStruct.FilterLen = 512;                /* Hall�ź������˲����� 512��ʱ������ */
    HALL_InitStruct.ClockDivision = HALL_CLK_DIV1;  /* ����Hallģ��ʱ�ӷ�Ƶϵ�� */
    HALL_InitStruct.Filter75_Ena = DISABLE;         /* Hall�ź��˲���ʽ��7��5ģʽ����ȫ1��Чģʽ */
    HALL_InitStruct.HALL_Ena = ENABLE;              /* ģ��ʹ�� */
    HALL_InitStruct.Capture_IRQ_Ena = ENABLE;       /* ��׽�ж�ʹ�� */
    HALL_InitStruct.OverFlow_IRQ_Ena = ENABLE;      /* ��ʱ�ж�ʹ�� */
    HALL_InitStruct.CountTH = 9600000;              /* Hallģ�����ģֵ����������ģֵ�������ʱ�ж� */

    HALL_Init(&HALL_InitStruct);
}

/*******************************************************************************
 �������ƣ�    void SoftDelay(void)
 ����������    ������ʱ����
 ���������    ��
 ���������    ��
 �� �� ֵ��    ��
 ����˵����
 �޸�����      �汾��          �޸���            �޸�����
 -----------------------------------------------------------------------------
 *******************************************************************************/
void SoftDelay(u32 cnt)
{
    volatile u32 t_cnt;

    for (t_cnt = 0; t_cnt < cnt; t_cnt++)
    {
        __nop();
    }
}


