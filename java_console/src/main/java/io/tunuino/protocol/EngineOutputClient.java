/* Copyright (c) 2026 Tunuino. Explicit actuator protocol; no commands on connect. */
package io.tunuino.protocol;
import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
public final class EngineOutputClient {
    private EngineOutputClient() {}
    private static final byte[] PREFIX="TUNUINO:X".getBytes(StandardCharsets.US_ASCII);
    private static byte[] exchange(PacketSession session,byte[] command)throws IOException {
        byte[] request=ByteBuffer.allocate(9+command.length).put(PREFIX).put(command).array();
        byte[] body=session.exchange(request,Duration.ofSeconds(3));
        if(body!=null&&body.length==1&&body[0]==(byte)0x83)
            throw new UnsupportedOperationException("Este firmware não inclui o comando de saídas; instala o Bundle com controlo do motor");
        if(body==null||body.length==0||body[0]!=0)throw new IOException("Comando recusado pela ECU: confirma o perfil, a calibração guardada e o estado desarmado");
        return body;
    }
    public record State(int mode,int faults,long active,long pending,boolean configured,int cylinders,
                        boolean timerReady,long generation,long injectionUs,boolean persisted,boolean autonomous) {
        public boolean armed(){return mode==1||mode==2||mode==4;}
        public String faultsDescription(){
            if(faults==0)return "sem falhas";
            var reasons=new java.util.ArrayList<String>();
            if((faults&1)!=0)reasons.add("atraso de temporização");
            if((faults&2)!=0)reasons.add("watchdog/comunicação expirada");
            if((faults&4)!=0)reasons.add("perda de sincronização ou sinal inválido");
            if((faults&8)!=0)reasons.add("calibração inválida ou alterada");
            if((faults&16)!=0)reasons.add("pressão de turbo acima do limite");
            if((faults&32)!=0)reasons.add("proteção de temperatura ou tensão");
            return String.join(", ",reasons);
        }
        public String activeChannels(){
            var names=new java.util.ArrayList<String>();
            for(int i=0;i<28;++i)if((active&(1L<<i))!=0)names.add(i<16?"LS "+(i+1):"IGN "+(i-15));
            return names.isEmpty()?"nenhuma":String.join(", ",names);
        }
        public String description(){return switch(mode){
            case 0->"DESARMADO";case 1->"ARMADO · aguarda sincronização 720°";
            case 2->"CONTROLO SEQUENCIAL ATIVO";case 3->"CORTE POR FALHA";case 4->"PULSO DE BANCADA";
            default->throw new IllegalStateException();};}
    }
    public static State decode(byte[] b)throws IOException {
        if(b==null||b.length!=24||b[0]!=0||b[1]!=1||(b[2]&255)>4||(b[3]&192)!=0
            ||(b[12]&255)>1||(b[13]&255)>12||(b[14]&255)>1||(b[15]&252)!=0)
            throw new IOException("Estado de saídas inválido");
        ByteBuffer p=ByteBuffer.wrap(b);long active=Integer.toUnsignedLong(p.getInt(4));
        long pending=Integer.toUnsignedLong(p.getInt(8)),injection=Integer.toUnsignedLong(p.getInt(20));
        if((active&0xf0000000L)!=0||(pending&0xf0000000L)!=0||injection>20000)
            throw new IOException("Máscara/tempo de saídas inválido");
        return new State(b[2],b[3],active,pending,b[12]!=0,b[13],b[14]!=0,Integer.toUnsignedLong(p.getInt(16)),injection,(b[15]&1)!=0,(b[15]&2)!=0);
    }
    public static State read(PacketSession session)throws IOException {return decode(exchange(session,new byte[]{0}));}
    private static void accepted(byte[] body)throws IOException {if(body.length!=1)throw new IOException("Confirmação de comando inválida");}
    public static State configure(PacketSession s,byte[] profile)throws IOException {
        if(profile.length!=128||profile[0]!=1)throw new IllegalArgumentException("Perfil deve ter 128 bytes, formato 1");
        accepted(exchange(s,ByteBuffer.allocate(129).put((byte)1).put(profile).array()));return read(s);
    }
    public static State arm(PacketSession s,long generation)throws IOException {
        if(generation<1||generation>0xffffffffL)throw new IllegalArgumentException("Geração de configuração inválida");
        accepted(exchange(s,ByteBuffer.allocate(9).put((byte)2).put("ARM!".getBytes(StandardCharsets.US_ASCII)).putInt((int)generation).array()));return read(s);
    }
    public static State stop(PacketSession s)throws IOException {accepted(exchange(s,new byte[]{3}));return read(s);}
    public static State pulse(PacketSession s,int channel,int microseconds)throws IOException {
        if(channel<0||channel>=28||microseconds<1||microseconds>(channel<16?20000:5000))
            throw new IllegalArgumentException("Pulso excede os limites da saída");
        accepted(exchange(s,ByteBuffer.allocate(8).put((byte)4).put((byte)channel).putShort((short)microseconds)
            .put("TEST".getBytes(StandardCharsets.US_ASCII)).array()));return read(s);
    }
    public static State auxiliaryTest(PacketSession s,int channel,int milliseconds)throws IOException {
        if(channel<0||channel>=16||milliseconds<1||milliseconds>800)
            throw new IllegalArgumentException("Teste auxiliar limitado a LS 1–16 e 1–800 ms");
        accepted(exchange(s,ByteBuffer.allocate(8).put((byte)5).put((byte)channel).putShort((short)milliseconds)
            .put("AUX!".getBytes(StandardCharsets.US_ASCII)).array()));return read(s);
    }
}
