/* Copyright (c) 2026 Tunuino. */
package io.tunuino.protocol;

import io.tunuino.calibration.IoConfiguration;
import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Arrays;
import java.util.List;
import java.util.ArrayList;
import java.util.zip.CRC32;

/** Exclusive serial-worker transactions. Stage, validate, burn and verify settings. */
public final class ConfigurationClient {
    private ConfigurationClient(){}
    public record Validation(boolean dirty,boolean persisted,int errors,long generation,long pageCrc) {
        public String explanation() {
            List<String> issues=new ArrayList<>();
            String[] names={"esquema/reservas","cambota/limite RPM","MAP","lambda","temperatura","referência ADC","mapa VE","mapa de ignição","perfil de motor","modo autónomo: perfil, sensores e mapa VE completo","correções de arranque/aquecimento"};
            for(int i=0;i<names.length;i++)if((errors&(1<<i))!=0)issues.add(names[i]);
            return issues.isEmpty()?"Configuração validada pelo firmware":String.join(" · ",issues);
        }
    }
    public record Snapshot(IoConfiguration configuration,Validation validation){}

    public static Validation decodeValidation(byte[] body)throws IOException {
        if(body==null||body.length!=16||body[0]!=0||body[1]!=1||(body[2]&255)>1||(body[3]&255)>1)throw new IOException("Diagnóstico de configuração incompatível");
        ByteBuffer bytes=ByteBuffer.wrap(body);long errors=Integer.toUnsignedLong(bytes.getInt(4)),generation=Integer.toUnsignedLong(bytes.getInt(8));
        if(errors>2047||generation==0)throw new IOException("Diagnóstico de configuração inválido");
        return new Validation(body[2]!=0,body[3]!=0,(int)errors,generation,Integer.toUnsignedLong(bytes.getInt(12)));
    }
    private static Duration remaining(long deadline)throws IOException {
        long nanos=deadline-System.nanoTime();if(nanos<=0)throw new IOException("Tempo de configuração excedido");
        return Duration.ofNanos(Math.min(nanos,Duration.ofSeconds(6).toNanos()));
    }
    private static void requireProfile(DeviceIdentity identity) {
        if(!EnginePreviewClient.supports(identity))throw new IllegalStateException("A configuração requer Proteus v0.7 F427 IO");
    }
    private static void accepted(byte[] body)throws IOException {
        if(body==null||body.length!=1||body[0]!=0)throw new IOException(body!=null&&body.length==1&&body[0]==(byte)0x80
            ?"O firmware não confirmou a persistência na flash":"A ECU recusou a operação de configuração");
    }
    public static Validation validation(PacketSession session,DeviceIdentity identity,Duration timeout)throws IOException {
        requireProfile(identity);
        byte[] body=session.exchange("TUNUINO:V".getBytes(StandardCharsets.US_ASCII),timeout);
        if(body.length==1&&body[0]==(byte)0x83)throw new IOException("Atualiza o firmware com o mesmo Bundle da Console para configurar as entradas");
        return decodeValidation(body);
    }
    private static Snapshot read(PacketSession session,DeviceIdentity identity,long deadline)throws IOException {
        requireProfile(identity);
        Validation before=validation(session,identity,remaining(deadline));
        byte[] page=new byte[256];
        for(int offset=0;offset<256;offset+=64) {
            byte[] query=ByteBuffer.allocate(7).put((byte)'r').putShort((short)1).putShort((short)offset).putShort((short)64).array();
            byte[] body=session.exchange(query,remaining(deadline));
            if(body.length!=65||body[0]!=0)throw new IOException("Leitura da configuração incompleta");
            System.arraycopy(body,1,page,offset,64);
        }
        Validation after=validation(session,identity,remaining(deadline));
        CRC32 crc=new CRC32();crc.update(page);
        if(!before.equals(after)||crc.getValue()!=after.pageCrc())throw new IOException("A configuração mudou durante a leitura; lê novamente");
        return new Snapshot(new IoConfiguration(page),after);
    }
    public static Snapshot read(PacketSession session,DeviceIdentity identity,Duration timeout)throws IOException {
        if(timeout.isNegative()||timeout.isZero()||timeout.compareTo(Duration.ofSeconds(30))>0)throw new IllegalArgumentException("Prazo de leitura inválido");
        return read(session,identity,System.nanoTime()+timeout.toNanos());
    }
    public static Snapshot apply(PacketSession session,DeviceIdentity identity,Snapshot expected,
            IoConfiguration candidate,Duration timeout)throws IOException {
        requireProfile(identity);
        if(timeout.compareTo(Duration.ofSeconds(1))<0||timeout.compareTo(Duration.ofSeconds(30))>0)throw new IllegalArgumentException("Prazo de configuração inválido");
        if(!candidate.errors().isEmpty())throw new IllegalArgumentException(String.join(" · ",candidate.errors()));
        if(expected.validation().dirty())throw new IOException("Há alterações pendentes na ECU; não foram substituídas");
        long deadline=System.nanoTime()+timeout.toNanos();
        Snapshot current=read(session,identity,deadline);
        if(current.validation().dirty()||current.validation().generation()!=expected.validation().generation()
                ||!Arrays.equals(current.configuration().bytes(),expected.configuration().bytes()))throw new IOException("A configuração da ECU mudou; lê antes de aplicar");
        byte[] desired=candidate.bytes();
        if(Arrays.equals(desired,current.configuration().bytes()))return current;
        boolean burnAttempted=false;
        try {
            for(int offset=0;offset<256;offset+=64) {
                byte[] query=ByteBuffer.allocate(71).put((byte)'w').putShort((short)1).putShort((short)offset).putShort((short)64).put(desired,offset,64).array();
                accepted(session.exchange(query,remaining(deadline)));
            }
            Snapshot staged=read(session,identity,deadline);
            if(staged.validation().errors()!=0)throw new IOException("Validação do firmware: "+staged.validation().explanation());
            if(staged.validation().generation()!=expected.validation().generation()||!Arrays.equals(desired,staged.configuration().bytes()))throw new IOException("A configuração preparada não corresponde ao pedido");
            burnAttempted=true;
            accepted(session.exchange(new byte[]{'b',0,1},remaining(deadline)));
            Snapshot verified=read(session,identity,deadline);
            long generation=(expected.validation().generation()+1)&0xffffffffL;if(generation==0)generation=1;
            if(verified.validation().dirty()||!verified.validation().persisted()||verified.validation().generation()!=generation
                    ||!Arrays.equals(desired,verified.configuration().bytes()))throw new IOException("Leitura após gravação não confirmou os valores enviados");
            return verified;
        }catch(IOException error) {
            if(!burnAttempted&&session.isOpen())try{accepted(session.exchange(new byte[]{'D'},Duration.ofMillis(500)));}catch(IOException discard){error.addSuppressed(discard);}
            if(burnAttempted)throw new IOException("Confirma a configuração por nova leitura antes de repetir: "+error.getMessage(),error);
            throw error;
        }
    }
    public static Snapshot discard(PacketSession session,DeviceIdentity identity,Duration timeout)throws IOException {
        requireProfile(identity);
        if(timeout.isNegative()||timeout.isZero()||timeout.compareTo(Duration.ofSeconds(30))>0)throw new IllegalArgumentException("Prazo de configuração inválido");
        long deadline=System.nanoTime()+timeout.toNanos();
        accepted(session.exchange(new byte[]{'D'},remaining(deadline)));return read(session,identity,deadline);
    }
}
