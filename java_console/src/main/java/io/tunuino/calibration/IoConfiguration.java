/* Copyright (c) 2026 Tunuino. */
package io.tunuino.calibration;

import java.io.IOException;
import java.math.BigDecimal;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.zip.CRC32;

/** Immutable F427 IO schema-2 settings; VE/ignition live on separate 16x16 pages. */
public final class IoConfiguration {
    public enum Field {
        WHEEL("Aquisição","Dentes nominais",3,1,4,120,0),
        MISSING("Aquisição","Dentes em falta",4,1,1,59,0),
        RPM_LIMIT("Aquisição","Limite de RPM",96,2,1000,20000,0),
        REFERENCE("Aquisição","Referência ADC (mV)",100,2,2800,3600,0),
        MAP_RAW_LOW("MAP","ADC inferior",10,2,0,4095,0),
        MAP_RAW_HIGH("MAP","ADC superior",12,2,0,4095,0),
        MAP_LOW("MAP","Pressão inferior (kPa)",14,2,0,50000,2),
        MAP_HIGH("MAP","Pressão superior (kPa)",16,2,0,50000,2),
        LAMBDA_RAW_LOW("Lambda","ADC inferior",18,2,0,4095,0),
        LAMBDA_RAW_HIGH("Lambda","ADC superior",20,2,0,4095,0),
        LAMBDA_LOW("Lambda","Lambda inferior",22,2,0,5000,3),
        LAMBDA_HIGH("Lambda","Lambda superior",24,2,0,5000,3),
        TEMP_RAW_1("Temperatura","ADC ponto 1",32,2,0,4095,0),
        TEMP_RAW_2("Temperatura","ADC ponto 2",34,2,0,4095,0),
        TEMP_RAW_3("Temperatura","ADC ponto 3",36,2,0,4095,0),
        TEMP_1("Temperatura","Temperatura 1 (°C)",64,2,-4000,15000,2),
        TEMP_2("Temperatura","Temperatura 2 (°C)",66,2,-4000,15000,2),
        TEMP_3("Temperatura","Temperatura 3 (°C)",68,2,-4000,15000,2);
        public final String group,label;
        final int offset,width,min,max,decimals;
        Field(String group,String label,int offset,int width,int min,int max,int decimals) {
            this.group=group;this.label=label;this.offset=offset;this.width=width;
            this.min=min;this.max=max;this.decimals=decimals;
        }
    }
    private static final byte[] MAGIC="TNCFG002".getBytes(StandardCharsets.US_ASCII);
    private final byte[] bytes;
    public IoConfiguration(byte[] bytes) {
        if(bytes==null||bytes.length!=256)throw new IllegalArgumentException("A configuração IO requer 256 bytes");
        this.bytes=bytes.clone();
    }
    public byte[] bytes(){return bytes.clone();}
    public byte[] engineProfile(){return Arrays.copyOfRange(bytes,128,256);}
    public boolean autonomous(){return bytes[102]==1;}
    public IoConfiguration withEngine(byte[] profile,boolean autonomous){
        if(profile==null||profile.length!=128)throw new IllegalArgumentException("Perfil de motor inválido");
        byte[] updated=bytes.clone();updated[102]=(byte)(autonomous?1:0);
        System.arraycopy(profile,0,updated,128,128);
        IoConfiguration candidate=new IoConfiguration(updated);
        if(!candidate.errors().isEmpty())throw new IllegalArgumentException(String.join(" · ",candidate.errors()));
        return candidate;
    }
    public static IoConfiguration localDefaults(){
        byte[] p=new byte[256];ByteBuffer b=ByteBuffer.wrap(p);
        b.putShort(0,(short)2);p[3]=36;p[4]=1;p[26]=3;
        b.putShort(12,(short)4095).putShort(20,(short)4095).putShort(34,(short)2048).putShort(36,(short)4095)
            .putShort(96,(short)12000).putShort(100,(short)3300);
        return new IoConfiguration(p);
    }
    public boolean triggerEnabled(){return bytes[2]==1;}
    public int mapChannel(){return bytes[6]&255;}
    public int lambdaChannel(){return bytes[7]&255;}
    public int coolantChannel(){return bytes[8]&255;}
    private int raw(Field field) {
        if(field.width==1)return bytes[field.offset]&255;
        int value=ByteBuffer.wrap(bytes).getShort(field.offset);
        return field.min<0?value:value&65535;
    }
    public String value(Field field){return BigDecimal.valueOf(raw(field),field.decimals).stripTrailingZeros().toPlainString();}

    public List<String> errors() {
        List<String> errors=new ArrayList<>();
        if(bytes[0]!=0||bytes[1]!=2||bytes[5]!=0||bytes[9]!=0)errors.add("Esquema/reservas incompatíveis");
        for(int i=27;i<32;i++)if(bytes[i]!=0)errors.add("Reservas de calibração alteradas");
        for(int i=98;i<100;i++)if(bytes[i]!=0)errors.add("Reservas de aquisição alteradas");
        for(int i=103;i<128;i++)if(bytes[i]!=0)errors.add("Reservas de configuração alteradas");
        byte[] profile=engineProfile();boolean hasProfile=!EngineProfile.empty(profile);
        if(hasProfile&&!EngineProfile.valid(profile,bytes[3]&255,bytes[4]&255))errors.add("Perfil de motor inválido: verifica saídas, PMS, fase e limites");
        if((bytes[102]&255)>1||(autonomous()&&(!hasProfile||!triggerEnabled()||mapChannel()==0||coolantChannel()==0)))
            errors.add("Funcionamento autónomo exige perfil, aquisição, MAP e temperatura configurados");
        for(Field field:Field.values())if(raw(field)<field.min||raw(field)>field.max)errors.add(field.label+": fora dos limites");
        if((bytes[2]&255)>1||raw(Field.MISSING)>=raw(Field.WHEEL)/2)errors.add("Roda/dentes em falta incompatíveis");
        if(mapChannel()>11||raw(Field.MAP_RAW_LOW)>=raw(Field.MAP_RAW_HIGH)
                ||(mapChannel()!=0&&raw(Field.MAP_LOW)>=raw(Field.MAP_HIGH)))errors.add("Curva/canal MAP inválido");
        if(lambdaChannel()>11||raw(Field.LAMBDA_RAW_LOW)>=raw(Field.LAMBDA_RAW_HIGH)
                ||(lambdaChannel()!=0&&(raw(Field.LAMBDA_LOW)==0||raw(Field.LAMBDA_LOW)>=raw(Field.LAMBDA_HIGH))))errors.add("Curva/canal lambda inválido");
        if(coolantChannel()>4||bytes[26]!=3||raw(Field.TEMP_RAW_1)>=raw(Field.TEMP_RAW_2)
                ||raw(Field.TEMP_RAW_2)>=raw(Field.TEMP_RAW_3)
                ||(coolantChannel()!=0&&raw(Field.TEMP_1)==raw(Field.TEMP_2)&&raw(Field.TEMP_2)==raw(Field.TEMP_3)))errors.add("Curva/canal de temperatura inválido");
        for(int i=3;i<16;i++)if(bytes[32+2*i]!=0||bytes[33+2*i]!=0||bytes[64+2*i]!=0||bytes[65+2*i]!=0)errors.add("Pontos de temperatura reservados alterados");
        return List.copyOf(errors.stream().distinct().toList());
    }

    public IoConfiguration edited(Map<Field,String> values,boolean trigger,int map,int lambda,int coolant) {
        if(map<0||map>11||lambda<0||lambda>11||coolant<0||coolant>4)throw new IllegalArgumentException("Canal de entrada inválido");
        byte[] updated=bytes.clone();updated[2]=(byte)(trigger?1:0);updated[6]=(byte)map;updated[7]=(byte)lambda;updated[8]=(byte)coolant;
        for(var entry:values.entrySet()) {
            Field field=entry.getKey();String text=entry.getValue().trim();
            try {
                if(text.length()>32||!text.matches("[+-]?[0-9]+(?:\\.[0-9]+)?"))throw new NumberFormatException();
                int value=new BigDecimal(text).movePointRight(field.decimals).intValueExact();
                if(value<field.min||value>field.max)throw new ArithmeticException();
                if(field.width==1)updated[field.offset]=(byte)value;
                else ByteBuffer.wrap(updated).putShort(field.offset,(short)value);
            }catch(ArithmeticException|NumberFormatException error){throw new IllegalArgumentException(field.label+": usa um valor dentro dos limites e até "+field.decimals+" casas decimais, sem arredondamento",error);}
        }
        IoConfiguration candidate=new IoConfiguration(updated);
        if(!candidate.errors().isEmpty())throw new IllegalArgumentException(String.join(" · ",candidate.errors()));
        return candidate;
    }

    public void writeNew(Path file)throws IOException {
        if(!errors().isEmpty())throw new IOException(String.join(" · ",errors()));
        byte[] packet=ByteBuffer.allocate(270).put(MAGIC).putShort((short)256).put(bytes).array();
        CRC32 crc=new CRC32();crc.update(packet,0,266);ByteBuffer.wrap(packet).putInt(266,(int)crc.getValue());
        Files.write(file,packet,StandardOpenOption.CREATE_NEW,StandardOpenOption.WRITE);
    }
    public static IoConfiguration read(Path file)throws IOException {
        byte[] data;
        try(var input=Files.newInputStream(file)){data=input.readNBytes(271);}
        if(data.length!=270||!Arrays.equals(Arrays.copyOf(data,8),MAGIC)||Short.toUnsignedInt(ByteBuffer.wrap(data).getShort(8))!=256)throw new IOException("Ficheiro de configuração Tunuino incompatível");
        CRC32 crc=new CRC32();crc.update(data,0,266);
        if(crc.getValue()!=Integer.toUnsignedLong(ByteBuffer.wrap(data).getInt(266)))throw new IOException("CRC da configuração incorreto");
        IoConfiguration config=new IoConfiguration(Arrays.copyOfRange(data,10,266));
        if(!config.errors().isEmpty())throw new IOException(String.join(" · ",config.errors()));
        return config;
    }
}
