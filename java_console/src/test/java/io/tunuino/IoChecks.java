/* Copyright (c) 2026 Tunuino. */
package io.tunuino;
import io.tunuino.protocol.*;
import io.tunuino.transport.*;
import io.tunuino.calibration.CalibrationMap;
import io.tunuino.datalog.*;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

public final class IoChecks {
    private static void check(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    private static byte[] exchange(PacketSession session,byte[] request)throws IOException{return session.exchange(request,Duration.ofSeconds(5));}
    private static void accepted(byte[] response){check(Arrays.equals(response,new byte[]{0}),"Command accepted");}
    private static void put16(byte[] p,int offset,int value){ByteBuffer.wrap(p).putShort(offset,(short)value);}
    private static void configuration(PacketSession session,byte[] settings)throws Exception{
        for(int offset=0;offset<settings.length;){int count=Math.min(64,settings.length-offset);
            byte[] q=ByteBuffer.allocate(7+count).put((byte)'w').putShort((short)1).putShort((short)offset).putShort((short)count).put(settings,offset,count).array();
            accepted(exchange(session,q));offset+=count;
        }
        accepted(exchange(session,new byte[]{'b',0,1}));
    }
    public static void main(String[] args)throws Exception{
        Path directory=Files.createTempDirectory("tunuino-io-");
        try(PacketSession session=new PacketSession(new ProcessSerialChannel(List.of(args[0])))){
            DeviceIdentity identity=IdentityClient.identify(session,DeviceIdentity.Family.PROTEUS,Duration.ofSeconds(3));
            check(identity.capabilities()==15&&identity.firmwareVersion().equals("0.2.0"),"IO identity");
            IoClient.Inputs initial=IoClient.read(session,identity,Duration.ofSeconds(2));
            check(initial.sample().validMask()==0&&(initial.status()&1)!=0&&initial.raw()[0]==2000,"Raw ADC without invented calibrated data");
            int[] isolated=initial.raw();isolated[0]=0;check(initial.raw()[0]==2000,"Immutable ADC snapshot");
            byte[] rawRange=IoClient.readRange(session,identity,60,32,Duration.ofSeconds(2));
            for(int j=0;j<16;++j)check(Short.toUnsignedInt(ByteBuffer.wrap(rawRange).getShort(j*2))==2000,
                "Ranged IO read preserves physical ADC values");
            try { IoClient.readRange(session,identity,99,2,Duration.ofSeconds(2));
                throw new AssertionError("Out-of-range IO query accepted");
            } catch(IllegalArgumentException expected) {
                check(session.isOpen(),"Invalid local query must not close USB session");
            }
            EngineTimingClient.Timing unavailable=EngineTimingClient.read(session,identity,Duration.ofSeconds(2));
            check(!unavailable.valid() && (unavailable.reasons()&EnginePreviewClient.TRIGGER_DISABLED)!=0
                && unavailable.revolutionUs()==0 && unavailable.relativeLoadPercent()==0,
                "Disabled settings must not produce theoretical timing");
            EnginePreviewClient.Preview disabled=EnginePreviewClient.read(session,identity,Duration.ofSeconds(2));
            check(!disabled.valid() && (disabled.reasons()&EnginePreviewClient.TRIGGER_DISABLED)!=0
                && (disabled.reasons()&EnginePreviewClient.NO_RPM)!=0
                && disabled.explanation().contains("RPM"),"Factory map preview is explicitly unavailable");
            byte[] settings=new byte[256];
            for(int offset=0;offset<256;offset+=64){byte[] q=ByteBuffer.allocate(7).put((byte)'r').putShort((short)1).putShort((short)offset).putShort((short)64).array();
                byte[] response=exchange(session,q);check(response.length==65&&response[0]==0,"Configuration chunk read");System.arraycopy(response,1,settings,offset,64);}
            settings[2]=1;settings[6]=1;settings[7]=2;settings[8]=1;
            put16(settings,10,1000);put16(settings,12,3000);put16(settings,14,0);put16(settings,16,20000);
            put16(settings,18,1000);put16(settings,20,3000);put16(settings,22,500);put16(settings,24,1500);
            put16(settings,32,1000);put16(settings,34,2000);put16(settings,36,3000);
            put16(settings,64,10000);put16(settings,66,5000);put16(settings,68,0);
            configuration(session,settings);
            IoClient.Inputs calibrated=IoClient.read(session,identity,Duration.ofSeconds(2));
            check(calibrated.sample().validMask()==15&&calibrated.sample().rpm()==1000&&calibrated.sample().mapKpa()==100
                &&calibrated.sample().lambda()==1&&calibrated.sample().coolantC()==50,"C calibration through real Java client");
            CalibrationMap map=MapReadClient.read(session,identity,CalibrationMap.Kind.VE,Duration.ofSeconds(3));
            map=map.withCell(0,0,"77.25");MapWriteClient.write(session,identity,map,Duration.ofSeconds(10));
            check(Arrays.equals(map.encode(),MapReadClient.read(session,identity,map.kind(),Duration.ofSeconds(3)).encode()),"Persist/map readback contract");
            CalibrationMap ignition=MapReadClient.read(session,identity,CalibrationMap.Kind.IGNITION,Duration.ofSeconds(3)).withCell(15,15,"-12.34");
            MapWriteClient.write(session,identity,ignition,Duration.ofSeconds(10));
            check(Arrays.equals(ignition.encode(),MapReadClient.read(session,identity,ignition.kind(),Duration.ofSeconds(3)).encode()),"Signed ignition map write/readback");
            EnginePreviewClient.Preview uncalibrated=EnginePreviewClient.read(session,identity,Duration.ofSeconds(2));
            check(!uncalibrated.valid() && (uncalibrated.reasons()&EnginePreviewClient.UNCALIBRATED)!=0,
                "Sparse test maps cannot advertise valid engine calculations");
            // Synthetic constant maps exercise the complete C -> USB -> Java path.
            // These test values must NEVER be used as an engine tune.
            CalibrationMap fullVe=MapReadClient.read(session,identity,CalibrationMap.Kind.VE,Duration.ofSeconds(3));
            CalibrationMap fullIgn=MapReadClient.read(session,identity,CalibrationMap.Kind.IGNITION,Duration.ofSeconds(3));
            for(int row=0;row<16;++row)for(int col=0;col<16;++col) {
                fullVe=fullVe.withCell(row,col,"100.00");
                fullIgn=fullIgn.withCell(row,col,"-5.00");
            }
            MapWriteClient.write(session,identity,fullVe,Duration.ofSeconds(10));
            MapWriteClient.write(session,identity,fullIgn,Duration.ofSeconds(10));
            EnginePreviewClient.Preview computed=EnginePreviewClient.read(session,identity,Duration.ofSeconds(2));
            check(computed.valid() && computed.reasons()==0 && computed.vePercent()==100
                && computed.advanceDegrees()==-5 && computed.rpm()==1000
                && computed.mapKpa()==100 && computed.generation()>0,
                "Real C firmware returns valid VE and ignition preview to Java");
            EngineTimingClient.Timing timing=EngineTimingClient.read(session,identity,Duration.ofSeconds(2));
            check(timing.valid() && timing.reasons()==0 && timing.rpm()==1000
                && timing.revolutionUs()==60000 && timing.cycleUs()==120000
                && timing.wheelSlotUs()==1667 && timing.relativeLoadPercent()==100
                && timing.sparkReferenceUs()==833 && timing.generation()>0,
                "Real C F427 dry-run crank timing and relative load decoded by Java");
            byte[] malformedTiming=exchange(session,
                "TUNUINO:T".getBytes(java.nio.charset.StandardCharsets.US_ASCII));
            malformedTiming[13]^=1; // Cycle high byte: guaranteed to break 2 × revolution contract.
            try{EngineTimingClient.decode(malformedTiming);
                throw new AssertionError("Corrupted cycle period was accepted");}
            catch(IOException expected){check(session.isOpen(),"Local decode keeps healthy link");}
            try{EngineTimingClient.decode(new byte[]{(byte)0x83});
                throw new AssertionError("Unsupported firmware response decoded as data");}
            catch(IOException expected){check(session.isOpen(),"No fabricated unsupported data");}
            byte[] malformedPreview=exchange(session,"TUNUINO:E".getBytes(java.nio.charset.StandardCharsets.US_ASCII));
            malformedPreview[2]=0;
            try { EnginePreviewClient.decode(malformedPreview);
                throw new AssertionError("Contradictory engine preview accepted");
            } catch(IOException expected) { check(session.isOpen(),"Local decode error must not disrupt USB"); }
            try{MapWriteClient.encode(map.withCell(0,0,"77.251"));throw new AssertionError("Rounded map write");}
            catch(IllegalArgumentException expected){check(session.isOpen(),"Precision rejected before write");}
            settings[2]=0;configuration(session,settings);
            EngineTimingClient.Timing noSyncTiming=EngineTimingClient.read(session,identity,Duration.ofSeconds(2));
            check(!noSyncTiming.valid() && (noSyncTiming.reasons()&EnginePreviewClient.NO_SYNC)!=0
                && noSyncTiming.sparkReferenceUs()==0,"Loss of sync removes dry-run timing");
            EnginePreviewClient.Preview stopped=EnginePreviewClient.read(session,identity,Duration.ofSeconds(2));
            check(!stopped.valid()&&(stopped.reasons()&EnginePreviewClient.TRIGGER_DISABLED)!=0
                &&stopped.vePercent()==0&&stopped.advanceDegrees()==0,
                "Disabling trigger invalidates engine preview without changing maps");
            LogSample partial=LiveDataClient.read(session,identity,Duration.ofSeconds(2)).orElseThrow();
            check(partial.validMask()==14&&partial.rpm()==0&&!LogChannel.RPM.available(partial)&&LogChannel.RPM.format(partial).equals("—"),"Partial data is unavailable, not zero RPM");
            Path csv=directory.resolve("simulated-partial.csv");LiveDataMonitor monitor=new LiveDataMonitor(session,identity,Duration.ofMillis(10));
            var started=monitor.startRecording(csv);AtomicBoolean running=new AtomicBoolean(true);AtomicInteger samples=new AtomicInteger();
            AtomicInteger extraCount=new AtomicInteger();
            long count=monitor.run(running::get,value->{check(value.orElseThrow().validMask()==14,"Monitor partial mask");if(samples.incrementAndGet()==3)running.set(false);},error->{throw new AssertionError(error);},extra->{
                check(extra.sensors().generation()>0&&extra.outputs().generation()>0&&extra.profile().length==128,"Native TPS/IAT/voltage, outputs and profile share the same serial polling worker");
                byte[] copy=extra.profile();copy[0]=99;check(extra.profile()[0]!=99,"Auxiliary telemetry does not expose a mutable wiring profile");extraCount.incrementAndGet();
            });
            check(extraCount.get()==0,"Legacy IO peer does not advertise native auxiliary telemetry; polling falls back without closing the session");
            check(started.isDone()&&count==3&&session.isOpen(),"Independent IO monitoring/CSV");
            List<LogSample> read=CsvLogReader.read(csv);check(read.size()==3&&read.get(0).validMask()==14&&Files.readString(csv).startsWith(CsvLogReader.PARTIAL_HEADER),"Partial CSV roundtrip");
            check(Files.readString(csv).contains(",,100.0,1.0,50.0,14"),"Missing RPM is an empty CSV field");
            check(Double.isNaN(DatalogAnalysis.range(read,LogChannel.RPM).minimum()),"No fabricated RPM range");
            List<LogSample> series=java.util.stream.IntStream.range(0,10000).mapToObj(i->new LogSample(i,i==5000?0:1000,0,0,0,i==5000?0:1)).toList();
            var reduced=DatalogAnalysis.chartSamples(series,LogChannel.RPM,100);check(reduced.size()<=100&&reduced.stream().anyMatch(v->v.validMask()==0),"Graph reduction preserves missing-data breaks");
            byte[] invalid=exchange(session,new byte[]{'r',0,1,1,0,0,1});check(Arrays.equals(invalid,new byte[]{(byte)0x83}),"Out-of-bounds page rejected");
            check(IoClient.read(session,identity,Duration.ofSeconds(2)).sample().validMask()==14,"Invalid command preserves session");
            byte[] snapshot=exchange(session,new byte[]{'A'});snapshot[39]=32;
            try{IoClient.decode(snapshot,identity);throw new AssertionError("Bad IO mask accepted");}catch(IOException expected){/* strict layout */}
        }finally{try(var files=Files.list(directory)){for(Path path:files.toList())Files.delete(path);}Files.delete(directory);}
        System.out.println("F427 IO: C/Java identity, raw ADC, calibrated channels, configuration, map write/readback, strict precision, partial CSV and graph gaps passed. Synthetic host model.");
    }
}
