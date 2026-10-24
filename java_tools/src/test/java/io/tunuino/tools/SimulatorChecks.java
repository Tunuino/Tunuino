/* Copyright (c) 2026 Tunuino. */
package io.tunuino.tools;

import io.tunuino.calibration.CalibrationMap;
import io.tunuino.datalog.CsvLogReader;
import io.tunuino.protocol.DeviceIdentity;
import io.tunuino.protocol.IdentityClient;
import io.tunuino.protocol.LiveDataClient;
import io.tunuino.protocol.MapReadClient;
import io.tunuino.transport.PacketSession;
import io.tunuino.transport.ProcessSerialChannel;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.Arrays;
import java.util.List;

public final class SimulatorChecks {
    private SimulatorChecks() {}
    private static void check(boolean value) { if (!value) throw new AssertionError(); }
    public static void main(String[] args) throws Exception { Thread.sleep(10000); }
    public static void run(Path exe, Path directory) throws Exception {
        for (String family : List.of("proteus","uaefi")) {
            var channel = ProcessSerialChannel.simulator(exe,family,"normal");
            try (var session = new PacketSession(channel)) {
                var expected = family.equals("proteus") ? DeviceIdentity.Family.PROTEUS : DeviceIdentity.Family.UAEFI;
                var identity = IdentityClient.identify(session,expected,Duration.ofSeconds(2));
                check(identity.uid().equals("53494d554c41544f52303031") && identity.capabilities()==3);
                var sample = LiveDataClient.read(session,identity,Duration.ofSeconds(2)).orElseThrow();
                check(Math.abs(sample.rpm()-1000)<1 && sample.mapKpa()==60 && sample.coolantC()==85);
                for (var kind : CalibrationMap.Kind.values()) {
                    var map = MapReadClient.read(session,identity,kind,Duration.ofSeconds(3));
                    check(map.rows()==4 && map.columns()==4 && map.rpmAt(3)==6000);
                }
            }
            check(!channel.isAlive());
        }
        for (String scenario : List.of("normal","stall","sensor-loss","wrap")) {
            Path csv = directory.resolve("simulated-"+scenario+".csv");
            Tools.simulate(exe,csv,20,"proteus",scenario);
            var data = CsvLogReader.read(csv);
            check(data.size()==20 && data.get(19).timeMs()>data.get(0).timeMs());
            check(Files.readString(csv.resolveSibling(csv.getFileName()+".provenance.json")).contains("\"synthetic\":true"));
        }
        Path custom = directory.resolve("custom.tnmap");
        var map = new CalibrationMap(CalibrationMap.Kind.VE,new int[]{0,6000},new int[]{20000,100000},new int[][]{{12345,23456},{34567,45678}});
        map.writeNew(custom);
        try (var session = new PacketSession(new ProcessSerialChannel(List.of(exe.toString(),"--stdio","--ve",custom.toString())))) {
            var identity = IdentityClient.identify(session,DeviceIdentity.Family.PROTEUS,Duration.ofSeconds(2));
            check(Arrays.equals(map.encode(),MapReadClient.read(session,identity,CalibrationMap.Kind.VE,Duration.ofSeconds(3)).encode()));
        }
        // A child that never answers must be stopped when the actual client deadline expires.
        Path java = Path.of(System.getProperty("java.home"),"bin",System.getProperty("os.name").startsWith("Windows")?"java.exe":"java");
        var blocked = new ProcessSerialChannel(List.of(java.toString(),"-cp",System.getProperty("java.class.path"),SimulatorChecks.class.getName()));
        try (var session = new PacketSession(blocked)) {
            try { IdentityClient.identify(session,DeviceIdentity.Family.PROTEUS,Duration.ofMillis(30)); throw new AssertionError("No timeout"); }
            catch (IOException expected) { check(!session.isOpen() && !blocked.isAlive()); }
        }
        System.out.println("Simulator: native C/Java identity, captures, live CSV, full maps, loaded calibration, recovery scenarios and bounded process cleanup passed. Synthetic data only.");
    }
}
