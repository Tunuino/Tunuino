/* Copyright (c) 2026 Tunuino. */
package io.tunuino.calibration;

import io.tunuino.protocol.MapWriteClient;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.Arrays;
import java.util.zip.CRC32;

/** Immutable complete calibration for Proteus v0.7 F427; opening never touches an ECU. */
public record EngineProject(IoConfiguration core,AuxConfiguration auxiliary,CalibrationMap ve,CalibrationMap ignition) {
    public static final int FILE_SIZE=2868;
    private static final byte[] MAGIC="TUNUINOP".getBytes(StandardCharsets.US_ASCII);
    public EngineProject {
        java.util.Objects.requireNonNull(core);java.util.Objects.requireNonNull(auxiliary);
        java.util.Objects.requireNonNull(ve);java.util.Objects.requireNonNull(ignition);
        var errors=new java.util.ArrayList<>(core.errors());errors.addAll(auxiliary.errors(core.engineProfile()));
        byte[] c=core.bytes();ByteBuffer coreData=ByteBuffer.wrap(c);var feature=new FeatureConfiguration(auxiliary.bytes());
        if(auxiliary.tpsChannel()>0&&(auxiliary.tpsChannel()==core.mapChannel()||auxiliary.tpsChannel()==core.lambdaChannel()))errors.add("TPS partilha entrada com MAP/lambda");
        if(auxiliary.idleMode()>0&&core.coolantChannel()==0)errors.add("Ralenti exige CLT calibrado");
        if(auxiliary.boostMode()>0&&(core.mapChannel()==0||Short.toUnsignedInt(coreData.getShort(16))<auxiliary.raw(AuxConfiguration.Field.BOOST_CUT)))errors.add("Turbo exige MAP e faixa de calibração adequada ao corte");
        if(feature.flags()!=0){
            if(EngineProfile.empty(core.engineProfile())||core.mapChannel()==0||core.coolantChannel()==0)errors.add("As funções ativas exigem perfil, MAP e CLT");
            if((feature.flags()&(8|16|32))!=0&&auxiliary.tpsChannel()==0)errors.add("Lambda/transientes ativos exigem TPS");
            if((feature.flags()&8)!=0&&core.lambdaChannel()==0)errors.add("Malha fechada exige lambda calibrada");
            if((feature.flags()&64)!=0&&feature.raw(FeatureConfiguration.Field.LIMIT_CUT)>=Short.toUnsignedInt(coreData.getShort(96)))errors.add("Limitador deve ficar abaixo do máximo de aquisição");
            if((feature.flags()&1)!=0&&feature.raw(FeatureConfiguration.Field.IAT_INPUT)==core.coolantChannel())errors.add("IAT e CLT exigem entradas independentes");
            var used=new java.util.HashSet<Integer>();for(int ch:new int[]{core.mapChannel(),core.lambdaChannel(),auxiliary.tpsChannel()})if(ch>0)used.add(ch);
            int[] masks={2|512,128,256};FeatureConfiguration.Field[] inputs={FeatureConfiguration.Field.VOLT_INPUT,FeatureConfiguration.Field.LAUNCH_INPUT,FeatureConfiguration.Field.FLEX_INPUT};
            for(int i=0;i<3;i++)if((feature.flags()&masks[i])!=0&&!used.add(feature.raw(inputs[i])))errors.add("Entrada analógica partilhada entre funções ativas");
        }
        if(!errors.isEmpty())throw new IllegalArgumentException(String.join(" · ",errors));
        if(ve.kind()!=CalibrationMap.Kind.VE||ignition.kind()!=CalibrationMap.Kind.IGNITION)
            throw new IllegalArgumentException("O projeto exige VE e ignição, sem trocar as tabelas");
        MapWriteClient.encode(ve);MapWriteClient.encode(ignition);
        if(core.autonomous())for(int row=0;row<16;row++)for(int col=0;col<16;col++)
            if(ve.cell(row,col)<=0)throw new IllegalArgumentException("Modo autónomo exige todas as células VE calibradas e positivas");
    }
    public byte[] page(int number){return switch(number){case 1->core.bytes();case 2->MapWriteClient.encode(ve);case 3->MapWriteClient.encode(ignition);case 4->auxiliary.bytes();default->throw new IllegalArgumentException("Página inválida");};}
    public String description(){
        byte[] p=core.engineProfile();int cylinders=EngineProfile.empty(p)?0:p[1]&255;
        String mode=EngineProfile.empty(p)?"sem perfil":switch(p[28]){case 0->"sequencial 720°";case 1->"semi-sequencial + wasted COP";case 2->"batch + wasted COP";case 3->"semi-sequencial + bobinas duplas";case 4->"batch + bobinas duplas";default->throw new IllegalStateException();};
        return "Proteus v0.7 F427 · "+cylinders+" cilindros · "+mode+"\nVE e ignição 16×16 · sensores e auxiliares incluídos\nAtivação: "+(core.autonomous()?"autónoma ao ligar a alimentação":"manual, supervisionada por USB");
    }
    public byte[] encode(){
        byte[] fuel=ve.encode(),spark=ignition.encode();ByteBuffer out=ByteBuffer.allocate(FILE_SIZE);
        out.put(MAGIC).put((byte)1).put((byte)1).put((byte)7).put((byte)1);
        out.put(core.bytes()).put(auxiliary.bytes()).putShort((short)fuel.length).putShort((short)spark.length).put(fuel).put(spark);
        CRC32 crc=new CRC32();crc.update(out.array(),0,out.position());out.putInt((int)crc.getValue());return out.array();
    }
    public static EngineProject decode(byte[] bytes)throws IOException {
        if(bytes==null||bytes.length!=FILE_SIZE||!Arrays.equals(Arrays.copyOf(bytes,8),MAGIC)
            ||bytes[8]!=1||bytes[9]!=1||bytes[10]!=7||bytes[11]!=1)throw new IOException("Projeto incompatível: requer Proteus v0.7 F427 e formato 1");
        ByteBuffer b=ByteBuffer.wrap(bytes);CRC32 crc=new CRC32();crc.update(bytes,0,bytes.length-4);
        if(crc.getValue()!=Integer.toUnsignedLong(b.getInt(bytes.length-4)))throw new IOException("CRC do projeto inválido");
        int fuel=Short.toUnsignedInt(b.getShort(524)),spark=Short.toUnsignedInt(b.getShort(526));
        if(fuel!=1168||spark!=1168)throw new IOException("As duas tabelas do projeto têm de ser 16×16");
        try{return new EngineProject(new IoConfiguration(Arrays.copyOfRange(bytes,12,268)),new AuxConfiguration(Arrays.copyOfRange(bytes,268,524)),
            CalibrationMap.decode(Arrays.copyOfRange(bytes,528,528+fuel)),CalibrationMap.decode(Arrays.copyOfRange(bytes,528+fuel,528+fuel+spark)));}
        catch(IllegalArgumentException error){throw new IOException(error.getMessage(),error);}
    }
    public static EngineProject read(Path file)throws IOException {try(var stream=Files.newInputStream(file)){return decode(stream.readNBytes(FILE_SIZE+1));}}
    public void writeNew(Path file)throws IOException {Files.write(file,encode(),StandardOpenOption.CREATE_NEW,StandardOpenOption.WRITE);}
}
