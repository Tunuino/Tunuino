/* Copyright (c) 2026 Tunuino. */
package io.tunuino.protocol;

import io.tunuino.datalog.LogSample;
import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.time.Duration;
import java.util.Arrays;

public final class IoClient {
    public static final long CAPABILITY=8;
    public record Inputs(LogSample sample, int status, int referenceMv, long generation, int tooth, long errors, int[] raw) {
        public Inputs { raw=raw.clone(); }
        @Override public int[] raw(){return raw.clone();}
    }
    private IoClient() {}
    public static Inputs decode(byte[] body,DeviceIdentity identity) throws IOException {
        if(body==null||body.length!=101||body[0]!=0)throw new IOException("Bloco de entradas da ECU inválido");
        DeviceIdentity reported=DeviceIdentity.decode(Arrays.copyOf(body,32));
        if(!reported.equals(identity)||identity.capabilities()!=15||identity.family()!=DeviceIdentity.Family.PROTEUS
            ||identity.mcuSeries()!=DeviceIdentity.McuSeries.STM32_F4||identity.hardwareRevision()!=7
            ||!identity.firmwareVersion().equals("0.2.0")||(body[32]&0x48)!=0||(body[32]&7)>4)throw new IOException("Perfil IO da ECU incompatível");
        ByteBuffer b=ByteBuffer.wrap(body,1,100).slice();
        int fraction=Short.toUnsignedInt(b.getShort(36)),mask=b.get(38)&255,status=b.get(39)&255;
        int reference=Short.toUnsignedInt(b.getShort(92));
        if(fraction>999||mask>15||status>15||((mask&14)!=0&&(status&1)==0)
            ||((mask&1)!=0&&(status&8)==0)||reference<2800||reference>3600||b.getInt(56)==0)
            throw new IOException("Metadados IO da ECU inválidos");
        int[] raw=new int[16];
        for(int i=0;i<16;++i){raw[i]=Short.toUnsignedInt(b.getShort(60+2*i));if(raw[i]>4095||((status&1)==0&&raw[i]!=0))throw new IOException("ADC inválido");}
        try {
            LogSample sample=new LogSample(Integer.toUnsignedLong(b.getInt(32))*1000+fraction,
                Integer.toUnsignedLong(b.getInt(40))/1000.0,Integer.toUnsignedLong(b.getInt(44))/1000.0,
                Integer.toUnsignedLong(b.getInt(48))/1_000_000.0,b.getInt(52)/1000.0,mask);
            return new Inputs(sample,status,reference,Integer.toUnsignedLong(b.getInt(56)),Short.toUnsignedInt(b.getShort(94)),Integer.toUnsignedLong(b.getInt(96)),raw);
        } catch(IllegalArgumentException error){throw new IOException(error.getMessage(),error);}
    }
    /** Read a contiguous runtime slice using the same O%2o%2c command as the INI. */
    public static byte[] readRange(PacketSession session, DeviceIdentity identity,
            int offset, int count, Duration timeout) throws IOException {
        if (identity == null || identity.capabilities() != 15
                || identity.family() != DeviceIdentity.Family.PROTEUS
                || identity.mcuSeries() != DeviceIdentity.McuSeries.STM32_F4
                || identity.hardwareRevision() != 7 || !identity.firmwareVersion().equals("0.2.0"))
            throw new IllegalStateException("O firmware não suporta leituras IO por blocos");
        if (offset < 0 || offset >= 100 || count < 1 || count > 100 - offset)
            throw new IllegalArgumentException("Intervalo de canais IO inválido");
        ByteBuffer query = ByteBuffer.allocate(5).put((byte) 'O')
            .putShort((short)offset).putShort((short)count);
        try {
            byte[] body = session.exchange(query.array(), timeout);
            if (body == null || body.length != count + 1 || body[0] != 0)
                throw new IOException("Resposta de leitura IO por blocos inválida");
            return Arrays.copyOfRange(body, 1, body.length);
        } catch (IOException error) {
            try { session.close(); }
            catch (IOException closing) { error.addSuppressed(closing); }
            throw error;
        }
    }

    public static Inputs read(PacketSession session,DeviceIdentity identity,Duration timeout) throws IOException {
        if((identity.capabilities()&CAPABILITY)==0)throw new IllegalStateException("O firmware não anuncia entradas IO");
        try{return decode(session.exchange(new byte[]{'A'},timeout),identity);}
        catch(IOException error){try{session.close();}catch(IOException closing){error.addSuppressed(closing);}throw error;}
    }
}
