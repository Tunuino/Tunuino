/* Copyright (c) 2026 Tunuino. */
package io.tunuino.protocol;

import io.tunuino.calibration.CalibrationMap;
import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.math.BigDecimal;
import java.nio.ByteBuffer;
import java.time.Duration;
import java.util.Arrays;

/** Serial worker only. One COM owner; writes a whole validated map, then verifies it. */
public final class MapWriteClient {
    private MapWriteClient() {}
    private static int exact(double value) {
        try { return BigDecimal.valueOf(value).movePointRight(2).intValueExact(); }
        catch(ArithmeticException error){throw new IllegalArgumentException("A ECU aceita até duas casas decimais nos mapas",error);}
    }
    public static byte[] encode(CalibrationMap map) {
        if(map.rows()!=16||map.columns()!=16)throw new IllegalArgumentException("O firmware IO requer mapas 16 × 16");
        ByteBuffer out=ByteBuffer.allocate(576);
        for(int i=0;i<16;++i){int rpm=map.rpmAt(i);if(rpm>20000)throw new IllegalArgumentException("Eixo RPM excede 20000");out.putShort((short)rpm);}
        for(int i=0;i<16;++i){int load=exact(map.loadKpaAt(i));if(load<0||load>50000)throw new IllegalArgumentException("Eixo MAP excede 500 kPa");out.putShort((short)load);}
        for(int row=0;row<16;++row)for(int col=0;col<16;++col){int cell=exact(map.cell(row,col));if(cell<Short.MIN_VALUE||cell>Short.MAX_VALUE)throw new IllegalArgumentException("Célula fora do formato da ECU");out.putShort((short)cell);}
        return out.array();
    }
    private static Duration remaining(long deadline) throws IOException {
        long nanos=deadline-System.nanoTime();if(nanos<=0)throw new IOException("Tempo de envio do mapa excedido");return Duration.ofNanos(Math.min(nanos,Duration.ofSeconds(6).toNanos()));
    }
    private static void accepted(byte[] response) throws IOException {
        if(response==null||response.length!=1||response[0]!=0)
            throw new IOException(response!=null&&response.length==1&&response[0]==(byte)0x84?"A ECU recusou a configuração/mapa inválido":"A ECU não confirmou a gravação");
    }
    public static void write(PacketSession session,DeviceIdentity identity,CalibrationMap map,Duration timeout) throws IOException {
        byte[] bytes=encode(map);
        if(identity.capabilities()!=15||!identity.firmwareVersion().equals("0.2.0"))throw new IllegalStateException("O firmware não suporta escrita de mapas IO");
        if(timeout.compareTo(Duration.ofSeconds(1))<0||timeout.compareTo(Duration.ofSeconds(30))>0)throw new IllegalArgumentException("Prazo de envio inválido");
        long deadline=System.nanoTime()+timeout.toNanos();
        IoClient.Inputs inputs=IoClient.read(session,identity,remaining(deadline));
        if((inputs.status()&2)!=0)throw new IOException("Existem alterações pendentes na ECU; aplica-as ou descarta-as antes de enviar o mapa");
        int page=map.kind()==CalibrationMap.Kind.VE?2:3;boolean burnAttempted=false;String phase="envio dos blocos do mapa";
        try {
            for(int offset=0;offset<bytes.length;){int count=Math.min(64,bytes.length-offset);
                ByteBuffer request=ByteBuffer.allocate(7+count).put((byte)'w').putShort((short)page).putShort((short)offset).putShort((short)count).put(bytes,offset,count);
                accepted(session.exchange(request.array(),remaining(deadline)));offset+=count;
            }
            phase="confirmação da gravação na flash";burnAttempted=true;
            accepted(session.exchange(new byte[]{'b',0,(byte)page},remaining(deadline)));
            phase="releitura do mapa guardado";
            CalibrationMap verified=MapReadClient.read(session,identity,map.kind(),remaining(deadline));
            if(!Arrays.equals(verified.encode(),map.encode()))throw new IOException("O mapa lido após a gravação não corresponde ao enviado");
        } catch(IOException error) {
            if(!burnAttempted&&session.isOpen())try{accepted(session.exchange(new byte[]{'D'},Duration.ofMillis(500)));}catch(IOException reset){error.addSuppressed(reset);}
            throw new IOException("Falha na "+phase+": "+error.getMessage()
                +(burnAttempted?". Confirma o mapa por nova leitura antes de repetir a gravação.":""),error);
        }
    }
}
