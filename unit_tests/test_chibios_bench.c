/* Execute the real ChibiOS bench main loop against a deterministic host model. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "board.h"
#include "usbcfg.h"
#include "tunuino/frame.h"
#include "tunuino/identity.h"
#include "board_io.h"

int tunuino_chibios_bench_main(void);
USBDriver USBD1;
SerialUSBDriver SDU1;
const USBConfig usbcfg = {0};
const SerialUSBConfig serusbcfg = {&USBD1,2,2,1};
volatile uint32_t tunuino_usb_generation;
volatile uint32_t tunuino_bench_fault;
static jmp_buf done;
static unsigned stage, writes, completions, restarts;
static uint32_t now, queued_at;
static bool rx_done, busy;
static const char *mode;
static uint8_t packet[64], queued[64], delivered[512];
static size_t packet_size, queued_size, delivered_size;
static eventflags_t flags;

void halInit(void) { assert(stage++ == 0); }
void chSysInit(void) { assert(stage++ == 1); }
void board_uid(uint8_t uid[12]) { for (unsigned i=0;i<12;++i) uid[i]=(uint8_t)(i+1); }
uint64_t board_millis(void) { return now; }
uint64_t board_io_time_us(void) { return (uint64_t)now*1000; }
uint64_t board_io_clock(void *user) { (void)user;return now; }
bool board_io_save(void *user,const uint8_t *bytes,size_t length) { (void)user;(void)bytes;(void)length;return false; }
void board_io_start(tunuino_device *device) { (void)device;assert(stage==2); }
void board_io_resync(tunuino_device *device) { (void)device;assert(stage==9); }
void board_io_update(tunuino_device *device) {
    uint16_t raw[16];for(unsigned i=0;i<16;++i)raw[i]=(uint16_t)(100+i);
    tunuino_device_sample(device,now,raw,true,0,false,0,0);
}
void tunuino_usb_serial_init(void) { assert(stage++ == 2); }
void sduObjectInit(SerialUSBDriver *sdu) { (void)sdu; assert(stage++ == 3); }
int sduStart(SerialUSBDriver *sdu, const SerialUSBConfig *config) {
  (void)sdu; assert(config->usbp == &USBD1); if(stage==4)++stage;else assert(stage==7); return MSG_OK;
}
void chEvtRegisterMaskWithFlags(void *s, event_listener_t *l, unsigned m, eventflags_t f) {
  (void)s;(void)l;(void)m;assert(f & CHN_TRANSMISSION_END);
}
void chSysLock(void) {}
void chSysUnlock(void) {}
eventflags_t chEvtGetAndClearFlags(event_listener_t *listener) {
  eventflags_t value=flags;(void)listener;flags=0;return value;
}
void usbDisconnectBus(USBDriver *usb) {
  assert(usb==&USBD1);
  if (stage==5) stage=6;
  else assert(stage==9);
}
void sduStop(SerialUSBDriver *sdu) { (void)sdu; ++restarts;stage=6; }
void usbStop(USBDriver *usb) { assert(usb==&USBD1);usb->state=0;busy=false; }
int usbStart(USBDriver *usb, const USBConfig *config) {
  assert(usb==&USBD1 && config==&usbcfg && stage==7 && now>=250);stage=8;return MSG_OK;
}
void usbConnectBus(USBDriver *usb) {
  assert(stage==8);stage=9;usb->state=USB_ACTIVE;++tunuino_usb_generation;
  if(restarts && strcmp(mode,"recover")==0){packet[packet_size-1]^=1;rx_done=false;}

}
void chThdSleepMilliseconds(uint32_t ms) {
  if (stage==6) { assert(ms==250);stage=7; }
  now+=ms;
  if (tunuino_bench_fault && strcmp(mode,"recover")!=0) longjmp(done,1);
  if (busy && strcmp(mode,"disconnect")==0 && now-queued_at>=3) {
    busy=false;USBD1.state=0;++tunuino_usb_generation;flags|=CHN_DISCONNECTED;
  }
  if (busy && strcmp(mode,"reconfigure")==0 && now-queued_at>=3) {
    busy=false;++tunuino_usb_generation;flags|=CHN_TRANSMISSION_END;
  }
  if (busy && (strcmp(mode,"normal")==0 || strcmp(mode,"tunerstudio")==0 || strcmp(mode,"recover")==0) && now-queued_at>=5) {
    memcpy(delivered+delivered_size,queued,queued_size);delivered_size+=queued_size;
    ++completions;busy=false;flags|=CHN_TRANSMISSION_END;
  }
  if ((strcmp(mode,"normal")==0 && completions==2)
      || (strcmp(mode,"tunerstudio")==0 && completions==9)
      || (strcmp(mode,"recover")==0 && restarts==1 && completions==1) || now>2000) longjmp(done,1);
}
size_t chnReadTimeout(SerialUSBDriver *sdu,uint8_t *bytes,size_t count,uint32_t timeout) {
  (void)sdu;(void)timeout;
  if (!rx_done && USBD1.state==USB_ACTIVE) {
    assert(packet_size<=count);memcpy(bytes,packet,packet_size);rx_done=true;return packet_size;
  }
  return 0;
}
size_t chnWriteTimeout(SerialUSBDriver *sdu,const uint8_t *bytes,size_t count,uint32_t timeout) {
  (void)sdu;(void)timeout;
  assert(!busy); /* Second response cannot start on queue acceptance alone. */
  assert(writes==completions);assert(count>0 && count<=63);
  memcpy(queued,bytes,count);queued_size=count;queued_at=now;busy=true;++writes;return count;
}

int main(int argc,char **argv) {
  size_t size;
  assert(argc==2);mode=argv[1];
  const uint8_t request[]="TUNUINO?";
  assert(tunuino_frame_encode(request,8,packet,sizeof(packet),&size)==TUNUINO_FRAME_OK);
  packet_size=size;
  if (strcmp(mode,"normal")==0) { memcpy(packet+size,packet,size);packet_size+=size; }
  if (strcmp(mode,"tunerstudio")==0) {
    packet[0]='F';packet[1]='Q';packet_size=2;
    for (const char *commands="SVAA";*commands;++commands) {
      assert(tunuino_frame_encode((const uint8_t *)commands,1,packet+packet_size,
          sizeof(packet)-packet_size,&size)==TUNUINO_FRAME_OK);
      packet_size+=size;
    }
    assert(tunuino_frame_encode(request,8,packet+packet_size,sizeof(packet)-packet_size,&size)==TUNUINO_FRAME_OK);
    packet_size+=size;
  }
  if (strcmp(mode,"corrupt")==0 || strcmp(mode,"recover")==0) packet[packet_size-1]^=1;
  if (setjmp(done)==0) (void)tunuino_chibios_bench_main();
  if (strcmp(mode,"normal")==0) {
    assert(writes==2 && completions==2 && tunuino_bench_fault==0);
    assert(delivered_size==2*(TUNUINO_IDENTITY_BODY_SIZE+TUNUINO_FRAME_OVERHEAD));
    for (size_t i=0;i<delivered_size;i+=delivered_size/2) {
      const uint8_t *body;size_t length;
      assert(tunuino_frame_decode(delivered+i,delivered_size/2,&body,&length)==TUNUINO_FRAME_OK);
      assert(length==TUNUINO_IDENTITY_BODY_SIZE && memcmp(body+1,"TUNUINO",7)==0);
    }
  } else if (strcmp(mode,"tunerstudio")==0) {
    assert(writes==9 && completions==9 && tunuino_bench_fault==0);
    assert(memcmp(delivered,"002Tunuino",10)==0);
    size_t offset=3;
    while (offset<delivered_size && delivered[offset]) ++offset;
    assert(offset<delivered_size); ++offset;
    for (unsigned i=0;i<5;++i) {
      assert(offset+2<=delivered_size);
      size_t length=(((size_t)delivered[offset]<<8)|delivered[offset+1])+TUNUINO_FRAME_OVERHEAD;
      assert(offset+length<=delivered_size);
      const uint8_t *body;size_t body_size;
      assert(tunuino_frame_decode(delivered+offset,length,&body,&body_size)==TUNUINO_FRAME_OK);
      assert(body[0]==0);
      if (i==2 || i==3) assert(body_size==101 && memcmp(body+1,"TUNUINO",7)==0);
      if (i==4) assert(body_size==32 && memcmp(body+1,"TUNUINO",7)==0);
      offset+=length;
    }
    assert(offset==delivered_size);
  } else if (strcmp(mode,"recover")==0) {
    assert(restarts==1&&writes==1&&completions==1&&tunuino_bench_fault==0&&USBD1.state==USB_ACTIVE);
  } else if (strcmp(mode,"corrupt")==0) assert(writes==0 && tunuino_bench_fault==20 && USBD1.state==0);
  else if (strcmp(mode,"timeout")==0) assert(writes==1 && completions==0 && tunuino_bench_fault==20 && USBD1.state==0);
  else if (strcmp(mode,"disconnect")==0 || strcmp(mode,"reconfigure")==0)
    assert(writes==1 && completions==0 && delivered_size==0 && tunuino_bench_fault==20 && USBD1.state==0);
  else assert(0);
  printf("ChibiOS bench host model: %s passed; real main loop, 250ms detach, TX completion and session isolation. No physical ECU.\n",mode);
  return 0;
}
