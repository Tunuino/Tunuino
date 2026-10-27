/* Copyright (c) 2026 Tunuino. Atomic assignments across configuration pages. */
package io.tunuino.protocol;
import io.tunuino.calibration.*;
import io.tunuino.transport.PacketSession;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.time.Duration;
import java.util.Arrays;
public final class IoAssignmentClient {
    private IoAssignmentClient(){}
    public record Snapshot(ConfigurationClient.Snapshot core,AuxControlClient.Snapshot auxiliary,EngineOutputClient.State engine){
        public IoAssignments assignments(){return new IoAssignments(core.configuration(),auxiliary.configuration());}
    }
    private static Duration remaining(long end)throws IOException{long n=end-System.nanoTime();if(n<=0)throw new IOException("Prazo de configuração excedido");return Duration.ofNanos(Math.min(n,Duration.ofSeconds(6).toNanos()));}
    private static void accepted(byte[] b)throws IOException{if(b==null||b.length!=1||b[0]!=0)throw new IOException("A ECU recusou a atribuição; confirma funções, calibrações e saídas desarmadas");}
    private static void routingOnly(byte[] original,byte[] candidate,boolean auxiliary){
        for(int i=0;i<256;i++){
            boolean allowed=auxiliary?(i==2||i==4||i==5||(i>=72&&i<=75)):(i>=6&&i<=8)||(i>=150&&i<=152)||(i>=160&&(i-160)%8<=1);
            if(!allowed&&original[i]!=candidate[i])throw new IllegalArgumentException("Este editor altera apenas atribuições; lê novamente a configuração");
        }
    }
    private static Snapshot read(PacketSession s,DeviceIdentity d,long end)throws IOException{
        var core=ConfigurationClient.read(s,d,remaining(end));var aux=AuxControlClient.read(s,d,remaining(end));var state=EngineOutputClient.read(s);
        var after=ConfigurationClient.validation(s,d,remaining(end));
        if(!core.validation().equals(aux.validation())||!after.equals(core.validation())||(state.configured()&&state.generation()!=after.generation()))throw new IOException("A configuração mudou durante a leitura; lê novamente");
        return new Snapshot(core,aux,state);
    }
    public static Snapshot read(PacketSession s,DeviceIdentity d,Duration timeout)throws IOException{return read(s,d,System.nanoTime()+timeout.toNanos());}
    public static Snapshot apply(PacketSession s,DeviceIdentity d,Snapshot expected,IoAssignments desired,Duration timeout)throws IOException{
        if(timeout.compareTo(Duration.ofSeconds(1))<0||timeout.compareTo(Duration.ofSeconds(30))>0)throw new IllegalArgumentException("Prazo inválido");
        byte[] core=desired.core().bytes(),aux=desired.auxiliary().bytes();long end=System.nanoTime()+timeout.toNanos();var before=read(s,d,end);
        routingOnly(expected.core().configuration().bytes(),core,false);routingOnly(expected.auxiliary().configuration().bytes(),aux,true);
        if(before.engine().armed())throw new IOException("Para todas as saídas antes de alterar funções");
        if(before.core().validation().dirty()||expected.core().validation().dirty()||!before.core().validation().equals(expected.core().validation())
            ||!Arrays.equals(before.core().configuration().bytes(),expected.core().configuration().bytes())||before.auxiliary().pageCrc()!=expected.auxiliary().pageCrc()
            ||!Arrays.equals(before.auxiliary().configuration().bytes(),expected.auxiliary().configuration().bytes()))throw new IOException("A configuração mudou ou contém alterações pendentes; lê novamente");
        if(Arrays.equals(core,before.core().configuration().bytes())&&Arrays.equals(aux,before.auxiliary().configuration().bytes()))return before;
        boolean committing=false;
        try{
            byte[][] pages={core,aux};int[] ids={1,4};
            for(int page=0;page<2;page++)for(int offset=0;offset<256;offset+=64){byte[] q=ByteBuffer.allocate(71).put((byte)'w').putShort((short)ids[page]).putShort((short)offset).putShort((short)64).put(pages[page],offset,64).array();accepted(s.exchange(q,remaining(end)));}
            var staged=read(s,d,end);if(staged.core().validation().errors()!=0||staged.core().validation().generation()!=before.core().validation().generation()
                ||!Arrays.equals(core,staged.core().configuration().bytes())||!Arrays.equals(aux,staged.auxiliary().configuration().bytes()))throw new IOException("A configuração preparada não passou na validação: "+staged.core().validation().explanation());
            committing=true;accepted(s.exchange(new byte[]{'b',0,1},remaining(end)));var saved=read(s,d,end);long generation=(before.core().validation().generation()+1)&0xffffffffL;if(generation==0)generation=1;
            if(saved.engine().armed()||saved.core().validation().dirty()||!saved.core().validation().persisted()||saved.core().validation().generation()!=generation
                ||!Arrays.equals(core,saved.core().configuration().bytes())||!Arrays.equals(aux,saved.auxiliary().configuration().bytes()))throw new IOException("A leitura após gravação não confirmou as atribuições");return saved;
        }catch(IOException e){if(!committing&&s.isOpen())try{accepted(s.exchange(new byte[]{'D'},Duration.ofMillis(500)));}catch(IOException discard){e.addSuppressed(discard);}
            if(committing)throw new IOException("Confirma as atribuições por nova leitura antes de repetir: "+e.getMessage(),e);throw e;}
    }
}
