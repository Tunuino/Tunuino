/* Copyright (c) 2026 Tunuino. Shared validation for motor settings and backups. */
package io.tunuino.calibration;
import java.nio.ByteBuffer;
import java.util.HashSet;
public final class EngineProfile {
    private EngineProfile(){}
    public static boolean empty(byte[] p){for(byte b:p)if(b!=0)return false;return true;}
    public static boolean valid(byte[] p,int slots,int missing){
        if(p==null||p.length!=128||(p[0]<1||p[0]>3)||p[1]<1||p[1]>12||slots<4||slots>120||missing<1||missing>=slots/2)return false;
        ByteBuffer b=ByteBuffer.wrap(p);
        int fuel=u(b,2),dwell=u(b,4),dead=u(b,6),maximum=u(b,8);
        if(fuel<100||fuel>30000||dwell<100||dwell>5000||dead>5000||maximum<100||maximum>20000||dead>=maximum
            ||u(b,10)>3000||b.getShort(14)<0||b.getShort(14)>=b.getShort(12)||b.getShort(12)>12000
            ||b.getShort(16)<-30000||b.getShort(16)>30000||Integer.toUnsignedLong(b.getInt(18))>=72000
            ||(p[25]&255)>(p[26]&255)||(p[26]&255)>=slots-missing||(p[27]&255)>1)return false;
        if((p[0]==1&&p[28]!=0)||(p[0]==2&&(p[28]<1||p[28]>2))
            ||(p[0]==3&&(p[28]<3||p[28]>4||p[1]%2!=0)))return false;
        for(int i=29;i<32;i++)if(p[i]!=0)return false;
        long used=0;var angles=new HashSet<Integer>();
        for(int i=0;i<12;i++){
            int at=32+8*i;
            if(i>=p[1]){for(int j=0;j<8;j++)if(p[at+j]!=0)return false;continue;}
            int injector=p[at]&255,coil=p[at+1]&255,tdc=b.getInt(at+2);
            if(injector>=16||coil>=12||tdc<0||tdc>=72000||p[at+6]!=0||p[at+7]!=0||!angles.add(tdc))return false;
            long mask=(1L<<injector)|(1L<<(16+coil));if((used&(1L<<injector))!=0||(!paired(p)&&(used&mask)!=0))return false;used|=mask;
        }
        if(paired(p))for(int i=0;i<p[1];i++){
            int partners=0;
            for(int j=0;j<p[1];j++)if(i!=j&&p[33+8*i]==p[33+8*j]){
                if(Math.abs(b.getInt(34+8*i)-b.getInt(34+8*j))!=36000)return false;
                partners++;
            }
            if(partners!=1)return false;
        }
        for(int at=22;at<=24;at++){
            int channel=p[at]&255;if(channel==0)continue;
            if(channel>16||(used&(1L<<(channel-1)))!=0)return false;used|=1L<<(channel-1);
        }
        return true;
    }
    public static boolean paired(byte[] p){return p!=null&&p.length==128&&p[0]==3;}
    public static int ignitionFunction(byte[] p,int cylinder){
        if(!paired(p))return cylinder+1;
        int first=cylinder;for(int i=0;i<(p[1]&255)&&i<12;i++)if(p[33+8*i]==p[33+8*cylinder])first=Math.min(first,i);
        return first+1;
    }
    public static String ignitionName(byte[] p,int cylinder){
        if(!paired(p))return "Ignição do cilindro "+(cylinder+1);
        int channel=p[33+8*cylinder]&255;var names=new java.util.ArrayList<String>();
        for(int i=0;i<(p[1]&255)&&i<12;i++)if((p[33+8*i]&255)==channel)names.add(Integer.toString(i+1));
        return "Bobina dos cilindros "+String.join(" + ",names);
    }
    private static int u(ByteBuffer b,int offset){return Short.toUnsignedInt(b.getShort(offset));}
}
