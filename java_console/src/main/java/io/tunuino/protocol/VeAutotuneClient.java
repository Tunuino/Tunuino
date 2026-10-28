/* Copyright (c) 2026 Tunuino. */
package io.tunuino.protocol;
import io.tunuino.calibration.*;
import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.nio.file.*;
import java.time.Duration;
import java.util.Arrays;

/** Generation-bound baseline and explicit, disarmed, backed-up VE commit. */
public final class VeAutotuneClient {
    private VeAutotuneClient(){}
    public record Base(DeviceIdentity identity,long generation,CalibrationMap map,IoConfiguration core,AuxConfiguration aux) {}
    public static Base read(PacketSession session,DeviceIdentity device)throws IOException {
        var core=ConfigurationClient.read(session,device,Duration.ofSeconds(5));
        if(core.validation().dirty()||!core.validation().persisted())throw new IOException("Guarda ou descarta as alterações da ECU antes de ler a base");
        var aux=AuxControlClient.read(session,device,Duration.ofSeconds(5));
        if(core.configuration().lambdaChannel()==0||aux.configuration().tpsChannel()==0)throw new IOException("Calibra primeiro lambda wideband e TPS");
        var feature=AuxControlClient.features(session);
        var map=MapReadClient.read(session,device,CalibrationMap.Kind.VE,Duration.ofSeconds(5));
        var after=ConfigurationClient.validation(session,device,Duration.ofSeconds(3));
        if(!after.equals(core.validation())||aux.validation().generation()!=after.generation()||feature.generation()!=after.generation())throw new IOException("A configuração mudou durante a leitura");
        return new Base(device,after.generation(),map,core.configuration(),aux.configuration());
    }
    public static void write(PacketSession session,DeviceIdentity device,Base expected,CalibrationMap proposed,Path backup)throws IOException {
        if(!device.equals(expected.identity()))throw new IOException("A ECU foi substituída; lê uma nova base");
        CalibrationMap original=expected.map();
        if(proposed.kind()!=CalibrationMap.Kind.VE||proposed.rows()!=original.rows()||proposed.columns()!=original.columns())throw new IllegalArgumentException("A proposta deve preservar a base VE");
        for(int i=0;i<original.columns();i++)if(original.rpmAt(i)!=proposed.rpmAt(i))throw new IllegalArgumentException("Eixo RPM alterado");
        for(int r=0;r<original.rows();r++){
            if(original.loadKpaAt(r)!=proposed.loadKpaAt(r))throw new IllegalArgumentException("Eixo MAP alterado");
            for(int c=0;c<original.columns();c++)if(Math.abs(proposed.cell(r,c)-original.cell(r,c))>Math.max(.005,original.cell(r,c)*.05)+.005)
                throw new IllegalArgumentException("Proposta excede o limite de 5% por sessão");
        }
        MapWriteClient.encode(proposed);
        if(EngineOutputClient.read(session).armed())throw new IOException("Pára as saídas antes de guardar VE");
        var validation=ConfigurationClient.validation(session,device,Duration.ofSeconds(3));
        if(validation.dirty()||!validation.persisted()||validation.generation()!=expected.generation())throw new IOException("A configuração mudou: a proposta não será gravada");
        var current=MapReadClient.read(session,device,CalibrationMap.Kind.VE,Duration.ofSeconds(5));
        if(!Arrays.equals(current.encode(),original.encode()))throw new IOException("O mapa VE foi alterado: lê uma nova base");
        Path parent=backup.toAbsolutePath().getParent();Files.createDirectories(parent);original.writeNew(backup);
        MapWriteClient.write(session,device,proposed,Duration.ofSeconds(15));
    }
}
