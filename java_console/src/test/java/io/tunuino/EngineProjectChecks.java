/* Copyright (c) 2026 Tunuino. */
package io.tunuino;
import io.tunuino.calibration.*;
import java.nio.ByteBuffer;
import java.nio.file.*;
import java.util.*;
import java.util.zip.CRC32;
public final class EngineProjectChecks {
    private static void check(boolean v,String message){if(!v)throw new AssertionError(message);}
    private static void invalid(byte[] bytes)throws Exception{try{EngineProject.decode(bytes);throw new AssertionError("Invalid project accepted");}catch(java.io.IOException expected){}}
    public static void main(String[] args)throws Exception {
        var core=IoConfiguration.localDefaults();var aux=new AuxConfiguration(new byte[256]);
        var ve=BaseMaps.create(BaseMaps.Template.NATURALLY_ASPIRATED,CalibrationMap.Kind.VE);
        var spark=BaseMaps.create(BaseMaps.Template.NATURALLY_ASPIRATED,CalibrationMap.Kind.IGNITION);
        var project=new EngineProject(core,aux,ve,spark);byte[] bytes=project.encode();
        check(bytes.length==2868&&ByteBuffer.wrap(bytes).getShort(524)==1168,"Independent header, page and map-file sizes");
        CRC32 crc=new CRC32();crc.update(bytes,0,2864);check(crc.getValue()==Integer.toUnsignedLong(ByteBuffer.wrap(bytes).getInt(2864)),"Independent complete-project CRC");
        check(Arrays.equals(EngineProject.decode(bytes).encode(),bytes),"All core, auxiliary, axes and cell bytes round-trip");
        byte[] corrupt=bytes.clone();corrupt[100]^=1;invalid(corrupt);invalid(Arrays.copyOf(bytes,bytes.length-1));invalid(Arrays.copyOf(bytes,bytes.length+1));
        for(int at:new int[]{8,9,10,11}){corrupt=bytes.clone();corrupt[at]=99;crc.reset();crc.update(corrupt,0,2864);ByteBuffer.wrap(corrupt).putInt(2864,(int)crc.getValue());invalid(corrupt);}
        try{new EngineProject(core,aux,spark,ve);throw new AssertionError("Swapped maps accepted");}catch(IllegalArgumentException expected){}
        Path dir=Files.createTempDirectory("tunuino-project-");Path file=dir.resolve("complete.tnproj");
        try{project.writeNew(file);check(Arrays.equals(EngineProject.read(file).encode(),bytes),"Bounded project-file reader");
            try{project.writeNew(file);throw new AssertionError("Project file overwritten");}catch(FileAlreadyExistsException expected){}
            check(Arrays.equals(Files.readAllBytes(file),bytes),"Existing file preserved");
        }finally{Files.delete(file);Files.delete(dir);}
        System.out.println("Engine projects: fixed-size full configuration, independent CRC/target checks, complete map/axis roundtrip, bounded reads and no file overwrite passed.");
    }
}
