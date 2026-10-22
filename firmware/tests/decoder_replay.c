// Host replay of the exact enabled decoder copies. File input only; no radio.
#include "connect_devices.h"
#include "decoder.h"
#include "pulse_slicer.h"
#include <stdlib.h>
#define DECL(name) &name,
static r_device const* const devices[] = {DEVICES};
#undef DECL
static unsigned messages;
static void output(r_device* decoder, data_t* data) {
  char json[16384];int truncated=0;
  data_append(data,"protocol","",DATA_STRING,decoder->name,NULL);
  data_print_jsons_checked(data,json,sizeof(json),&truncated);
  if(truncated)abort();
  puts(json);++messages;data_free(data);
}
static void quiet(r_device* decoder,int level,data_t* data){(void)decoder;(void)level;data_free(data);}
static void decode(pulse_data_t* pulses) {
  if(!pulses->num_pulses)return;
  pulses->sample_rate=1000000;
  for(unsigned i=0;i<sizeof(devices)/sizeof(*devices);++i){
    r_device device=*devices[i];device.output_fn=output;device.log_fn=quiet;
    switch(device.modulation){
      case OOK_PULSE_PWM:pulse_slicer_pwm(pulses,&device);break;
      case OOK_PULSE_PPM:pulse_slicer_ppm(pulses,&device);break;
      case OOK_PULSE_PCM:pulse_slicer_pcm(pulses,&device);break;
      case OOK_PULSE_MANCHESTER_ZEROBIT:pulse_slicer_manchester_zerobit(pulses,&device);break;
      case OOK_PULSE_DMC:pulse_slicer_dmc(pulses,&device);break;
      default:fprintf(stderr,"Unsupported replay modulation %d\n",device.modulation);exit(3);
    }
  }
  memset(pulses,0,sizeof(*pulses));
}
int main(int argc,char** argv){
  if(argc!=2)return 2;
  if(!strcmp(argv[1],"--registry")){
    for(unsigned i=0;i<sizeof(devices)/sizeof(*devices);++i)
      printf("%u\t%d\t%s\n",i,devices[i]->modulation,devices[i]->name);
    return 0;
  }
  FILE* file=fopen(argv[1],"r");if(!file)return 2;
  pulse_data_t pulses={0};char line[1024];
  while(fgets(line,sizeof(line),file)){
    if(!strncmp(line,";ook",4)||!strncmp(line,";end",4)){decode(&pulses);continue;}
    int pulse,gap;if(sscanf(line,"%d %d",&pulse,&gap)!=2)continue;
    if(pulse<0||gap<0||pulses.num_pulses>=PD_MAX_PULSES)return 3;
    pulses.pulse[pulses.num_pulses]=pulse;pulses.gap[pulses.num_pulses++]=gap;
  }
  decode(&pulses);fclose(file);return messages?0:1;
}
