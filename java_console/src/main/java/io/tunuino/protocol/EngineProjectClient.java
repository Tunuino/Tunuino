/* Copyright (c) 2026 Tunuino. */
package io.tunuino.protocol;

import io.tunuino.calibration.*;
import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.*;
import java.time.Duration;
import java.util.Arrays;

/** One owner, four staged pages, one Burn and complete verified readback. */
public final class EngineProjectClient {
    private EngineProjectClient(){}
    public record Snapshot(DeviceIdentity identity,EngineProject project,ConfigurationClient.Validation validation,EngineOutputClient.State engine) {}
    private static void supported(DeviceIdentity d){
        if(d.family()!=DeviceIdentity.Family.PROTEUS||d.mcuSeries()!=DeviceIdentity.McuSeries.STM32_F4||d.hardwareRevision()!=7
            ||d.capabilities()!=15||!d.firmwareVersion().equals("0.2.0"))throw new IllegalArgumentException("Projeto completo requer Proteus v0.7 F427 com o Bundle correspondente");
    }
    private static long deadline(Duration timeout){if(timeout.compareTo(Duration.ofSeconds(1))<0||timeout.compareTo(Duration.ofSeconds(90))>0)throw new IllegalArgumentException("Prazo inválido");return System.nanoTime()+timeout.toNanos();}
    private static Duration remaining(long end)throws IOException {long n=end-System.nanoTime();if(n<=0)throw new IOException("Tempo da operação de projeto excedido");return Duration.ofNanos(Math.min(n,Duration.ofSeconds(6).toNanos()));}
    private static void accepted(byte[] b)throws IOException {if(b==null||b.length!=1||b[0]!=0)throw new IOException("A ECU recusou o projeto; confirma compatibilidade, saídas desarmadas e valores válidos");}
    private static Snapshot read(PacketSession s,DeviceIdentity d,long end)throws IOException {
        supported(d);EngineOutputClient.read(s);
        var first=ConfigurationClient.validation(s,d,remaining(end));
        if(first.dirty())throw new IOException("Há alterações pendentes na ECU. Guarda ou descarta antes de ler/restaurar o projeto");
        var core=ConfigurationClient.read(s,d,remaining(end));var aux=AuxControlClient.read(s,d,remaining(end));
        EngineOutputClient.read(s); /* Renew manual supervision during a read-only transfer. */
        var ve=MapReadClient.read(s,d,CalibrationMap.Kind.VE,remaining(end));
        EngineOutputClient.read(s);
        var ignition=MapReadClient.read(s,d,CalibrationMap.Kind.IGNITION,remaining(end));
        var engine=EngineOutputClient.read(s);var after=ConfigurationClient.validation(s,d,remaining(end));
        if(!first.equals(after)||!core.validation().equals(after)||!aux.validation().equals(after)
            ||(engine.configured()&&engine.generation()!=after.generation()))throw new IOException("A configuração mudou durante a leitura do projeto");
        return new Snapshot(d,new EngineProject(core.configuration(),aux.configuration(),ve,ignition),after,engine);
    }
    public static Snapshot read(PacketSession s,DeviceIdentity d,Duration timeout)throws IOException{return read(s,d,deadline(timeout));}
    private static byte[] readPage(PacketSession s,int page,int size,long end)throws IOException {
        byte[] result=new byte[size];
        for(int offset=0;offset<size;offset+=64){int count=Math.min(64,size-offset);
            byte[] body=s.exchange(ByteBuffer.allocate(7).put((byte)'r').putShort((short)page).putShort((short)offset).putShort((short)count).array(),remaining(end));
            if(body.length!=count+1||body[0]!=0)throw new IOException("Releitura da página "+page+" incompleta");
            System.arraycopy(body,1,result,offset,count);
        }
        return result;
    }
    public static Snapshot apply(PacketSession s,DeviceIdentity d,Snapshot expected,EngineProject desired,Path backup,Duration timeout)throws IOException {
        supported(d);long end=deadline(timeout);
        if(!d.equals(expected.identity()))throw new IOException("A ECU foi substituída: lê o projeto desta centralina antes de restaurar");
        Snapshot before=read(s,d,end);
        if(before.engine().armed())throw new IOException("Pára todas as saídas antes de restaurar o projeto");
        if(!before.validation().equals(expected.validation())||!Arrays.equals(before.project().encode(),expected.project().encode()))
            throw new IOException("A configuração mudou: lê novamente antes de restaurar");
        Files.createDirectories(backup.toAbsolutePath().getParent());before.project().writeNew(backup);
        if(before.validation().persisted()&&Arrays.equals(before.project().encode(),desired.encode()))return before;
        boolean committing=false;String phase="preparação das quatro páginas";
        try{
            for(int page=1;page<=4;page++){
                byte[] data=desired.page(page);
                for(int offset=0;offset<data.length;offset+=64){int count=Math.min(64,data.length-offset);
                    accepted(s.exchange(ByteBuffer.allocate(7+count).put((byte)'w').putShort((short)page).putShort((short)offset).putShort((short)count).put(data,offset,count).array(),remaining(end)));
                }
            }
            phase="validação e releitura das páginas preparadas";
            var staged=ConfigurationClient.validation(s,d,remaining(end));
            if(staged.errors()!=0||staged.generation()!=before.validation().generation())throw new IOException("Projeto preparado inválido: "+staged.explanation());
            for(int page=1;page<=4;page++){
                byte[] requested=desired.page(page);
                if(!Arrays.equals(requested,readPage(s,page,requested.length,end)))throw new IOException("A página preparada não corresponde ao projeto");
            }
            phase="confirmação da gravação na flash";committing=true;
            accepted(s.exchange(new byte[]{'b',0,1},remaining(end)));
            phase="releitura do projeto guardado";var saved=read(s,d,end);
            long generation=(before.validation().generation()+1)&0xffffffffL;if(generation==0)generation=1;
            if(saved.engine().armed()||saved.validation().dirty()||!saved.validation().persisted()||saved.validation().generation()!=generation
                ||!Arrays.equals(saved.project().encode(),desired.encode()))throw new IOException("O projeto lido não confirmou a restauração completa");
            return saved;
        }catch(IOException failure){
            if(!committing&&s.isOpen())try{accepted(s.exchange(new byte[]{'D'},Duration.ofMillis(500)));}catch(IOException discard){failure.addSuppressed(discard);}
            throw new IOException("Falha na "+phase+": "+failure.getMessage()+(committing?". Confirma por nova leitura antes de repetir a gravação.":""),failure);
        }
    }
}
