/* Copyright (c) 2026 Tunuino. Immutable start/warm-up correction settings. */
package io.tunuino.calibration;
import java.nio.ByteBuffer;
public final class FuelCorrections {
    private final byte[] bytes;
    public FuelCorrections(byte[] bytes){if(bytes==null||bytes.length!=25)throw new IllegalArgumentException("Correções de combustível requerem 25 bytes");this.bytes=bytes.clone();}
    public byte[] bytes(){return bytes.clone();}
    public boolean enabled(){return bytes[0]==1;}
    public boolean configured(){for(byte b:bytes)if(b!=0)return true;return false;}
    public boolean fixedAdvance(){return bytes[23]==1;}
    public int value(int offset,boolean signed){int value=ByteBuffer.wrap(bytes).getShort(offset);return signed?value:value&65535;}
    public static FuelCorrections reference(){
        ByteBuffer p=ByteBuffer.allocate(25);p.put(0,(byte)1).putShort(1,(short)400).putShort(3,(short)15000)
            .putShort(5,(short)11000).putShort(7,(short)10000)
            .putShort(9,(short)-2000).putShort(11,(short)16000).putShort(13,(short)2000).putShort(15,(short)12000)
            .putShort(17,(short)8000).putShort(19,(short)10000).putShort(21,(short)1000);
        return new FuelCorrections(p.array());
    }
    public boolean valid(){
        if(!configured())return true;
        if((bytes[0]&255)>1||value(1,false)<100||value(1,false)>1000||value(3,false)<5000||value(3,false)>50000
            ||value(5,false)<10000||value(5,false)>30000||value(7,false)>60000
            ||(value(5,false)!=10000&&value(7,false)==0)||value(21,true)<-1000||value(21,true)>2000
            ||(bytes[23]&255)>1||bytes[24]!=0)return false;
        for(int i=0;i<3;i++){
            int temp=value(9+4*i,true),percent=value(11+4*i,false);
            if(temp<-4000||temp>15000||percent<5000||percent>50000||(i>0&&temp<=value(5+4*i,true)))return false;
        }
        return true;
    }
}
