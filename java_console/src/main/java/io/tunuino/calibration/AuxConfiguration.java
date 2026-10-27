/* Copyright (c) 2026 Tunuino. Page 4: TPS, idle PWM and boost PWM. */
package io.tunuino.calibration;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.io.IOException;
import java.math.BigDecimal;
import java.util.*;
import java.util.zip.CRC32;
public final class AuxConfiguration {
    public enum Field {
        TPS_CLOSED("TPS fechado (ADC)",6,0,0,4095),TPS_OPEN("TPS aberto (ADC)",8,0,0,4095),TPS_TOLERANCE("Tolerância fora dos extremos (% do curso ADC)",66,2,0,1000),
        IDLE_HZ("Frequência ralenti (Hz)",10,0,10,300),BOOST_HZ("Frequência turbo (Hz)",12,0,10,100),
        IDLE_MIN("Duty mínimo (%)",14,2,0,10000),IDLE_MAX("Duty máximo (%)",16,2,0,10000),
        CRANK_DUTY("Duty de arranque (%)",18,2,0,10000),IDLE_TPS("TPS máximo de ralenti (%)",20,2,0,2000),
        IDLE_RPM_MAX("Desativar ralenti acima de (rpm)",22,0,500,3000),IDLE_P("Proporcional (% / 100 rpm)",24,2,0,5000),IDLE_I("Integral (% / 100 rpm / s)",26,2,0,5000),
        TEMP1("Temperatura 1 (°C)",28,2,-4000,15000),TEMP2("Temperatura 2 (°C)",30,2,-4000,15000),TEMP3("Temperatura 3 (°C)",32,2,-4000,15000),
        TARGET1("Alvo ralenti 1 (rpm)",34,0,500,2000),TARGET2("Alvo ralenti 2 (rpm)",36,0,500,2000),TARGET3("Alvo ralenti 3 (rpm)",38,0,500,2000),
        DUTY1("Duty base 1 (%)",40,2,0,10000),DUTY2("Duty base 2 (%)",42,2,0,10000),DUTY3("Duty base 3 (%)",44,2,0,10000),
        CRANK_RPM("Arranque abaixo de (rpm)",46,0,100,1000),
        BOOST_MIN("Duty mínimo turbo (%)",48,2,0,9500),BOOST_MAX("Duty máximo turbo (%)",50,2,0,9500),BOOST_BASE("Duty base turbo (%)",52,2,0,9500),
        BOOST_TARGET("Alvo MAP absoluto (kPa)",54,2,10000,49999),BOOST_CUT("Corte por MAP absoluto (kPa)",56,2,10001,50000),
        BOOST_RPM("Ativar turbo acima de (rpm)",58,0,500,8000),BOOST_TPS("TPS mínimo turbo (%)",60,2,0,10000),
        BOOST_P("Proporcional (% / 10 kPa)",62,2,0,5000),BOOST_I("Integral (% / 10 kPa / s)",64,2,0,5000);
        public final String label;public final int offset,decimals,min,max;
        Field(String label,int offset,int decimals,int min,int max){this.label=label;this.offset=offset;this.decimals=decimals;this.min=min;this.max=max;}
    }
    private final byte[] bytes;
    public AuxConfiguration(byte[] bytes){if(bytes==null||bytes.length!=256)throw new IllegalArgumentException("A página de ralenti/turbo requer 256 bytes");this.bytes=bytes.clone();}
    public byte[] bytes(){return bytes.clone();}
    public int idleMode(){return bytes[1]&255;}public int idleChannel(){return bytes[2]&255;}
    public int boostMode(){return bytes[3]&255;}public int boostChannel(){return bytes[4]&255;}public int tpsChannel(){return bytes[5]&255;}
    public boolean configured(){return bytes[0]==1;}
    public int raw(Field f){int n=ByteBuffer.wrap(bytes).getShort(f.offset);return f.min<0?n:n&65535;}
    public String value(Field f){return BigDecimal.valueOf(raw(f),f.decimals).stripTrailingZeros().toPlainString();}
    public static AuxConfiguration reference(){
        byte[] p=new byte[256];p[0]=1;ByteBuffer b=ByteBuffer.wrap(p);
        int[][] fields={{6,400},{8,3600},{10,100},{12,25},{14,0},{16,8000},{18,3500},{20,200},{22,2200},{24,500},{26,100},
            {28,-2000},{30,2000},{32,8000},{34,1200},{36,1000},{38,850},{40,3500},{42,2500},{44,1800},{46,400},
            {48,0},{50,5000},{52,0},{54,15000},{56,20000},{58,2500},{60,5000},{62,200},{64,50},{66,500}};
        for(int[] f:fields)b.putShort(f[0],(short)f[1]);return new AuxConfiguration(p);
    }
    public List<String> errors(byte[] engineProfile){
        var errors=new ArrayList<String>();
        if(bytes[0]==0){for(byte value:bytes)if(value!=0){errors.add("Formato de ralenti/turbo inválido");break;}return errors;}
        if(bytes[0]!=1||idleMode()>2||boostMode()>2||idleChannel()>16||boostChannel()>16||tpsChannel()>11)errors.add("Modo ou canal de ralenti/turbo inválido");
        for(int i=68;i<256;i++)if(bytes[i]!=0){errors.add("Reservas de ralenti/turbo alteradas");break;}
        for(var f:Field.values())if(raw(f)<f.min||raw(f)>f.max)errors.add(f.label+": fora dos limites");
        if(raw(Field.TPS_CLOSED)==raw(Field.TPS_OPEN))errors.add("Calibrações TPS fechado/aberto devem ser diferentes");
        if(raw(Field.IDLE_MIN)>raw(Field.IDLE_MAX)||raw(Field.BOOST_MIN)>raw(Field.BOOST_MAX)||raw(Field.BOOST_BASE)<raw(Field.BOOST_MIN)||raw(Field.BOOST_BASE)>raw(Field.BOOST_MAX))errors.add("Limites de duty incompatíveis");
        if(raw(Field.CRANK_DUTY)<raw(Field.IDLE_MIN)||raw(Field.CRANK_DUTY)>raw(Field.IDLE_MAX))errors.add("Duty de arranque fora dos limites de ralenti");
        if(raw(Field.TEMP1)>=raw(Field.TEMP2)||raw(Field.TEMP2)>=raw(Field.TEMP3))errors.add("Temperaturas devem crescer");
        Field[] targets={Field.TARGET1,Field.TARGET2,Field.TARGET3},duties={Field.DUTY1,Field.DUTY2,Field.DUTY3};
        for(int i=0;i<3;i++)if(raw(targets[i])>=raw(Field.IDLE_RPM_MAX)||raw(targets[i])<=raw(Field.CRANK_RPM)||raw(duties[i])<raw(Field.IDLE_MIN)||raw(duties[i])>raw(Field.IDLE_MAX))errors.add("Curva de ralenti fora dos limites");
        if(raw(Field.BOOST_TARGET)>=raw(Field.BOOST_CUT))errors.add("Corte de pressão deve ser superior ao alvo");
        long used=0;
        if(engineProfile!=null&&engineProfile.length==128&&engineProfile[0]==1){
            for(int i=0;i<(engineProfile[1]&255)&&i<12;i++)used|=1L<<(engineProfile[32+8*i]&255);
            for(int i=22;i<=24;i++)if((engineProfile[i]&255)>0&&(engineProfile[i]&255)<=16)used|=1L<<((engineProfile[i]&255)-1);
        }
        int[] modes={idleMode(),boostMode()},channels={idleChannel(),boostChannel()};
        for(int i=0;i<2;i++)if(modes[i]>0){
            if(tpsChannel()==0||channels[i]==0)errors.add("Ralenti/turbo ativos exigem TPS e saída LS atribuída");
            if(channels[i]>0&&channels[i]<=16){long bit=1L<<(channels[i]-1);if((used&bit)!=0)errors.add("Saída LS partilhada com injetor, relé ou outra válvula");used|=bit;}
        }
        return List.copyOf(errors.stream().distinct().toList());
    }
    public AuxConfiguration edited(Map<Field,String> values,int idleMode,int idleChannel,int boostMode,int boostChannel,int tpsChannel){
        if(idleMode<0||idleMode>2||boostMode<0||boostMode>2||idleChannel<0||idleChannel>16||boostChannel<0||boostChannel>16||tpsChannel<0||tpsChannel>11)throw new IllegalArgumentException("Modo/canal inválido");
        byte[] p=bytes.clone();p[0]=1;p[1]=(byte)idleMode;p[2]=(byte)idleChannel;p[3]=(byte)boostMode;p[4]=(byte)boostChannel;p[5]=(byte)tpsChannel;
        for(var entry:values.entrySet()){
            Field f=entry.getKey();String text=entry.getValue().trim().replace(',','.');
            try{if(text.length()>32||!text.matches("[+-]?[0-9]+(?:\\.[0-9]+)?"))throw new ArithmeticException();int n=new BigDecimal(text).movePointRight(f.decimals).intValueExact();if(n<f.min||n>f.max)throw new ArithmeticException();ByteBuffer.wrap(p).putShort(f.offset,(short)n);}
            catch(RuntimeException e){throw new IllegalArgumentException(f.label+": valor inválido ou precisão excessiva");}
        }
        AuxConfiguration result=new AuxConfiguration(p);if(!result.errors(null).isEmpty())throw new IllegalArgumentException(String.join(" · ",result.errors(null)));return result;
    }
    public void writeNew(Path file)throws IOException{
        if(!errors(null).isEmpty())throw new IOException(String.join(" · ",errors(null)));
        byte[] packet=ByteBuffer.allocate(270).put("TNAUX001".getBytes(StandardCharsets.US_ASCII)).putShort((short)256).put(bytes).array();CRC32 crc=new CRC32();crc.update(packet,0,266);ByteBuffer.wrap(packet).putInt(266,(int)crc.getValue());Files.write(file,packet,StandardOpenOption.CREATE_NEW);
    }
    public static AuxConfiguration read(Path file)throws IOException{
        byte[] p;try(var input=Files.newInputStream(file)){p=input.readNBytes(271);}
        if(p.length!=270||!new String(p,0,8,StandardCharsets.US_ASCII).equals("TNAUX001")||Short.toUnsignedInt(ByteBuffer.wrap(p).getShort(8))!=256)throw new IOException("Cópia de ralenti/turbo incompatível");
        CRC32 crc=new CRC32();crc.update(p,0,266);if(crc.getValue()!=Integer.toUnsignedLong(ByteBuffer.wrap(p).getInt(266)))throw new IOException("CRC da cópia incorreto");
        AuxConfiguration result=new AuxConfiguration(Arrays.copyOfRange(p,10,266));if(!result.errors(null).isEmpty())throw new IOException(String.join(" · ",result.errors(null)));return result;
    }
}
