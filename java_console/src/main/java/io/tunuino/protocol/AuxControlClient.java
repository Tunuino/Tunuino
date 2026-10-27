/* Copyright (c) 2026 Tunuino. Exclusive page-4 transactions and read-only status. */
package io.tunuino.protocol;
import io.tunuino.calibration.AuxConfiguration;
import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Arrays;
import java.util.zip.CRC32;
public final class AuxControlClient {
    private AuxControlClient(){}
    public record Snapshot(AuxConfiguration configuration,ConfigurationClient.Validation validation,long pageCrc){}
    public record State(int idleMode,int boostMode,boolean tpsValid,boolean configured,int tps,int idleDuty,int boostDuty,
        int idleTarget,int boostTarget,long generation,int boostCut,boolean boostActive,boolean idleActive,long channels){}
    private static void accepted(byte[] b)throws IOException{if(b==null||b.length!=1||b[0]!=0)throw new IOException("A ECU recusou a configuração de ralenti/turbo; confirma perfil, sensores e saídas desarmadas");}
    private static Duration remaining(long deadline)throws IOException{long n=deadline-System.nanoTime();if(n<=0)throw new IOException("Tempo de configuração excedido");return Duration.ofNanos(Math.min(n,Duration.ofSeconds(6).toNanos()));}
    private static Snapshot read(PacketSession s,DeviceIdentity d,long deadline)throws IOException{
        var before=ConfigurationClient.validation(s,d,remaining(deadline));byte[] bytes=new byte[256];
        for(int offset=0;offset<256;offset+=64){byte[] q=ByteBuffer.allocate(7).put((byte)'r').putShort((short)4).putShort((short)offset).putShort((short)64).array();byte[] b=s.exchange(q,remaining(deadline));if(b.length!=65||b[0]!=0)throw new IOException("Atualiza firmware, INI e Console com o mesmo Bundle para configurar ralenti/turbo");System.arraycopy(b,1,bytes,offset,64);}
        byte[] crcReply=s.exchange(new byte[]{'k',0,4},remaining(deadline));if(crcReply.length!=5||crcReply[0]!=0)throw new IOException("CRC da página de ralenti/turbo indisponível");
        long expected=Integer.toUnsignedLong(ByteBuffer.wrap(crcReply).getInt(1));CRC32 crc=new CRC32();crc.update(bytes);
        var after=ConfigurationClient.validation(s,d,remaining(deadline));if(!before.equals(after)||crc.getValue()!=expected)throw new IOException("A configuração mudou durante a leitura; lê novamente");
        return new Snapshot(new AuxConfiguration(bytes),after,expected);
    }
    public static Snapshot read(PacketSession s,DeviceIdentity d,Duration timeout)throws IOException{return read(s,d,System.nanoTime()+timeout.toNanos());}
    public static Snapshot apply(PacketSession s,DeviceIdentity d,Snapshot expected,AuxConfiguration desired,Duration timeout)throws IOException{
        if(timeout.compareTo(Duration.ofSeconds(1))<0||timeout.compareTo(Duration.ofSeconds(30))>0)throw new IllegalArgumentException("Prazo inválido");
        if(!desired.errors(null).isEmpty())throw new IllegalArgumentException(String.join(" · ",desired.errors(null)));
        long deadline=System.nanoTime()+timeout.toNanos();Snapshot before=read(s,d,deadline);
        if(expected.validation().dirty()||before.validation().dirty()||before.validation().generation()!=expected.validation().generation()||before.pageCrc()!=expected.pageCrc()||!Arrays.equals(before.configuration().bytes(),expected.configuration().bytes()))throw new IOException("A configuração mudou ou contém alterações pendentes; lê antes de aplicar");
        if(Arrays.equals(before.configuration().bytes(),desired.bytes()))return before;
        boolean attempted=false;
        try{
            byte[] bytes=desired.bytes();for(int offset=0;offset<256;offset+=64){byte[] q=ByteBuffer.allocate(71).put((byte)'w').putShort((short)4).putShort((short)offset).putShort((short)64).put(bytes,offset,64).array();accepted(s.exchange(q,remaining(deadline)));}
            Snapshot staged=read(s,d,deadline);if(staged.validation().errors()!=0)throw new IOException("Validação: "+staged.validation().explanation());
            if(staged.validation().generation()!=before.validation().generation()||!Arrays.equals(bytes,staged.configuration().bytes()))throw new IOException("A página preparada não corresponde ao pedido");
            attempted=true;accepted(s.exchange(new byte[]{'b',0,4},remaining(deadline)));Snapshot saved=read(s,d,deadline);
            long generation=(before.validation().generation()+1)&0xffffffffL;if(generation==0)generation=1;
            if(saved.validation().dirty()||!saved.validation().persisted()||saved.validation().generation()!=generation||!Arrays.equals(bytes,saved.configuration().bytes()))throw new IOException("Leitura após gravação não confirmou os valores");return saved;
        }catch(IOException e){
            if(!attempted&&s.isOpen())try{accepted(s.exchange(new byte[]{'D'},Duration.ofMillis(500)));}catch(IOException discard){e.addSuppressed(discard);}
            if(attempted)throw new IOException("Confirma por nova leitura antes de repetir a gravação: "+e.getMessage(),e);throw e;
        }
    }
    public static State decode(byte[] b)throws IOException{
        if(b==null||b.length!=32||b[0]!=0||b[1]!=1||(b[2]&255)>2||(b[3]&255)>2||(b[4]&255)>1||(b[5]&255)>1||(b[22]&255)>1||(b[23]&255)>1)throw new IOException("Estado de ralenti/turbo inválido");
        ByteBuffer p=ByteBuffer.wrap(b);int tps=u(p,6),idle=u(p,8),boost=u(p,10);long gen=Integer.toUnsignedLong(p.getInt(16)),mask=Integer.toUnsignedLong(p.getInt(24));
        if(tps>10000||idle>10000||boost>9500||gen==0||mask>65535||p.getInt(28)!=0)throw new IOException("Valores de ralenti/turbo inválidos");
        return new State(b[2],b[3],b[4]!=0,b[5]!=0,tps,idle,boost,u(p,12),u(p,14),gen,u(p,20),b[22]!=0,b[23]!=0,mask);
    }
    private static int u(ByteBuffer p,int at){return Short.toUnsignedInt(p.getShort(at));}
    public static State state(PacketSession s)throws IOException{
        byte[] b=s.exchange("TUNUINO:X\u0007".getBytes(StandardCharsets.US_ASCII),Duration.ofSeconds(3));
        if(b.length==1&&b[0]!=0)throw new IOException("Estado indisponível: instala o Bundle com ralenti/turbo");return decode(b);
    }
}
