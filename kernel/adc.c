/* Copyright (C) 2024 Jisheng Zhang <jszhang@kernel.org> */

#include "config.h"
#include "adc.h"
#include "io.h"
#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "defs.h"
#include "proc.h"

#ifdef ADC_DRIVER

#define ADC_CTRL		0x04
#define  ADC_CTRL_EN		(1 << 0)
#define  ADC_CTRL_SEL_SFT	4
#define ADC_CYC_SET		0xc
#define  ADC_CYC_CLKDIV_SFT	12
#define  ADC_CYC_CLKDIV_MSK	0xf
#define ADC_CH_RESULT(n)	((n - 1) * 4 + 0x14)
#define  ADC_CH_RESULT_VALID	(1 << 15)
#define  ADC_CH_RESULT_SFT	0
#define  ADC_CH_RESULT_MSK	0xfff

struct adcchip {
  unsigned long base;
  struct spinlock lock;
};

struct adc {
  struct adcchip chip[ADC_NUM];
} adc;

static int adc_get(unsigned long arg)
{
  int chan, chip, ret, timeout = 1000;
  uint32 val;

  chan = (arg >> ADC_CHAN_SFT) & 0xff;
  chip = (arg >> ADC_CHIP_SFT) & 0xff;
  if (chip < 0 || chip > ADC_NUM - 1)
    return -1;
  if (chan < 1 || chan > 3)
    return -1;

  acquire(&adc.chip[chip].lock);

  val = (chan << (ADC_CTRL_SEL_SFT + 1)) | ADC_CTRL_EN;
  write32(adc.chip[chip].base + ADC_CTRL, val);
  for(;;) {
    val = read32(adc.chip[chip].base + ADC_CH_RESULT(chan));
    if (val & ADC_CH_RESULT_VALID) {
      ret = (val >> ADC_CH_RESULT_SFT) & ADC_CH_RESULT_MSK;
      break;
    }

    timeout--;
    if (!timeout) {
      printf("timeout to read adc chip:%d chan:%d\n", chip, chan);
      ret = -1;
      break;
    }
    kdelay(1);
  }

  release(&adc.chip[chip].lock);

  return ret;
}

static int adc_ioctl(int user_dst, unsigned long req, void *argp)
{
  switch(req) {
  case ADC_IOCTL_GET:
    return adc_get((unsigned long)argp);
  default:
    return -1;
  }
}

void adcinit(void)
{
  uint32 val;

  initlock(&adc.chip[0].lock, "adc0");
  adc.chip[0].base = ADC0;
  devsw[ADC].ioctl = adc_ioctl;

  val = read32(adc.chip[0].base + ADC_CYC_SET);
  val &= ~(ADC_CYC_CLKDIV_MSK << ADC_CYC_CLKDIV_SFT);
  val |= (0xf << ADC_CYC_CLKDIV_SFT);
  write32(adc.chip[0].base + ADC_CYC_SET, val);
}

#endif
