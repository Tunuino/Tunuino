/* Copyright (c) 2026 Tunuino. Function assignment without MCU pin names. */
package io.tunuino.calibration;
import java.util.*;
public final class IoAssignments {
    public enum Group {
        ANALOG("Entradas analógicas",11),TEMPERATURE("Temperaturas",4),POWER("Saídas de potência",16),IGNITION("Ignição",12);
        public final String label;public final int count;Group(String label,int count){this.label=label;this.count=count;}
    }
    public record Function(int id,String label){@Override public String toString(){return label;}}
    private final IoConfiguration originalCore;private final AuxConfiguration originalAux;
    private final EnumMap<Group,int[]> assignments=new EnumMap<>(Group.class);
    private final EnumMap<Group,Set<Integer>> overlaps=new EnumMap<>(Group.class);
    public IoAssignments(IoConfiguration core,AuxConfiguration aux){
        originalCore=Objects.requireNonNull(core);originalAux=Objects.requireNonNull(aux);
        for(var g:Group.values()){assignments.put(g,new int[g.count]);overlaps.put(g,new HashSet<>());}
        byte[] p=core.bytes(),a=aux.bytes();
        assign(Group.ANALOG,p[6]&255,1);assign(Group.ANALOG,p[7]&255,2);assign(Group.TEMPERATURE,p[8]&255,1);
        if(aux.configured()){
            assign(Group.ANALOG,a[5]&255,3);assign(Group.ANALOG,a[73]&255,4);assign(Group.ANALOG,a[75]&255,5);assign(Group.ANALOG,a[74]&255,6);
            assign(Group.TEMPERATURE,a[72]&255,2);assign(Group.POWER,a[2]&255,16);assign(Group.POWER,a[4]&255,17);
        }
        if(cylinders()>0){for(int i=0;i<cylinders();i++){assign(Group.POWER,(p[160+8*i]&255)+1,i+1);if(EngineProfile.ignitionFunction(core.engineProfile(),i)==i+1)assign(Group.IGNITION,(p[161+8*i]&255)+1,i+1);}
            assign(Group.POWER,p[150]&255,13);assign(Group.POWER,p[151]&255,14);assign(Group.POWER,p[152]&255,15);}
    }
    private IoAssignments(IoAssignments source){originalCore=source.originalCore;originalAux=source.originalAux;source.assignments.forEach((g,a)->assignments.put(g,a.clone()));source.overlaps.forEach((g,a)->overlaps.put(g,new HashSet<>(a)));}
    private void assign(Group g,int channel,int function){if(channel>0&&channel<=g.count){if(assignments.get(g)[channel-1]!=0)overlaps.get(g).add(channel-1);assignments.get(g)[channel-1]=function;}}
    public int cylinders(){byte[] p=originalCore.bytes();return (p[128]>=1&&p[128]<=3)&&(p[129]&255)<=12?p[129]&255:0;}
    public List<Function> functions(Group g){
        var f=new ArrayList<Function>();f.add(new Function(0,"Sem atribuição"));
        int count=cylinders()>0?cylinders():12;
        switch(g){
            case ANALOG->{f.add(new Function(1,"MAP — pressão do coletor"));f.add(new Function(2,"Lambda — mistura de combustível"));f.add(new Function(3,"TPS — posição da borboleta"));f.add(new Function(4,"Tensão da bateria"));f.add(new Function(5,"Etanol — flex fuel analógico"));f.add(new Function(6,"Comando de launch control"));}
            case TEMPERATURE->{f.add(new Function(1,"CLT — temperatura do motor"));f.add(new Function(2,"IAT — temperatura do ar"));}
            case POWER->{for(int i=1;i<=count;i++)f.add(new Function(i,"Injetor do cilindro "+i));f.add(new Function(13,"Bomba de combustível"));f.add(new Function(14,"Ventoinha de refrigeração"));f.add(new Function(15,"Relé principal"));f.add(new Function(16,"Válvula de ralenti PWM"));f.add(new Function(17,"Válvula de controlo do turbo"));}
            case IGNITION->{for(int i=1;i<=count;i++)if(cylinders()==0||EngineProfile.ignitionFunction(originalCore.engineProfile(),i-1)==i)f.add(new Function(i,cylinders()==0?"Ignição do cilindro "+i:EngineProfile.ignitionName(originalCore.engineProfile(),i-1)));}
        }
        return List.copyOf(f);
    }
    public ProteusRouting.Recommendation recommendation(Group group,int function){
        if(group==Group.IGNITION&&EngineProfile.paired(originalCore.engineProfile())){
            byte[] p=originalCore.engineProfile();var phases=new java.util.TreeSet<Integer>();var b=java.nio.ByteBuffer.wrap(p);
            for(int i=0;i<cylinders();i++)phases.add(b.getInt(34+8*i)%36000);
            int phase=b.getInt(34+8*(function-1))%36000;
            return new ProteusRouting.Recommendation(new java.util.ArrayList<>(phases).indexOf(phase)+1,false);
        }
        return ProteusRouting.recommendation(group,function);
    }
    public int function(Group g,int index){return assignments.get(g)[Objects.checkIndex(index,g.count)];}
    public String functionName(Group g,int id){return functions(g).stream().filter(f->f.id()==id).map(Function::label).findFirst().orElse("Função incompatível");}
    public static String terminal(Group g,int index){Objects.checkIndex(index,g.count);return switch(g){case ANALOG->"Entrada analógica "+(index+1);case TEMPERATURE->"Entrada de temperatura "+(index+1);case POWER->"Saída de potência "+(index+1);case IGNITION->"Saída de ignição "+(index+1);};}
    public static String connector(Group g,int index){Objects.checkIndex(index,g.count);return (g==Group.ANALOG?"AV":g==Group.TEMPERATURE?"AT":g==Group.POWER?"LS":"IGN")+(index+1);}
    /** Moving a function clears its former terminal. Other displaced functions stay unassigned. */
    public IoAssignments withFunction(Group g,int index,int id){
        Objects.checkIndex(index,g.count);if(functions(g).stream().noneMatch(f->f.id()==id))throw new IllegalArgumentException("Função incompatível com esta entrada ou saída");
        var next=new IoAssignments(this);int[] a=next.assignments.get(g);if(id!=0)for(int i=0;i<a.length;i++)if(a[i]==id)a[i]=0;a[index]=id;next.overlaps.get(g).remove(index);return next;
    }
    private int channel(Group g,int id){int[] a=assignments.get(g);for(int i=0;i<a.length;i++)if(a[i]==id)return i+1;return 0;}
    private byte[] coreBytes(){
        byte[] p=originalCore.bytes();p[6]=(byte)channel(Group.ANALOG,1);p[7]=(byte)channel(Group.ANALOG,2);p[8]=(byte)channel(Group.TEMPERATURE,1);
        if(cylinders()>0){for(int i=0;i<cylinders();i++){p[160+8*i]=(byte)Math.max(0,channel(Group.POWER,i+1)-1);p[161+8*i]=(byte)Math.max(0,channel(Group.IGNITION,EngineProfile.ignitionFunction(originalCore.engineProfile(),i))-1);}
            p[150]=(byte)channel(Group.POWER,13);p[151]=(byte)channel(Group.POWER,14);p[152]=(byte)channel(Group.POWER,15);}
        return p;
    }
    private byte[] auxBytes(){
        byte[] a=originalAux.bytes();a[5]=(byte)channel(Group.ANALOG,3);a[73]=(byte)channel(Group.ANALOG,4);a[75]=(byte)channel(Group.ANALOG,5);a[74]=(byte)channel(Group.ANALOG,6);
        a[72]=(byte)channel(Group.TEMPERATURE,2);a[2]=(byte)channel(Group.POWER,16);a[4]=(byte)channel(Group.POWER,17);return a;
    }
    public List<String> errors(){
        var e=new ArrayList<String>();
        if(cylinders()==0&&(Arrays.stream(assignments.get(Group.POWER)).anyMatch(n->n!=0)||Arrays.stream(assignments.get(Group.IGNITION)).anyMatch(n->n!=0)))e.add("Cria e guarda o perfil em Motor e saídas antes de atribuir funções aos atuadores");
        overlaps.forEach((g,rows)->rows.forEach(i->e.add(terminal(g,i)+": há funções sobrepostas na configuração lida; escolhe explicitamente uma função ou Sem atribuição")));
        for(int i=1;i<=cylinders();i++){if(channel(Group.POWER,i)==0)e.add("Falta saída para o injetor do cilindro "+i);if(channel(Group.IGNITION,EngineProfile.ignitionFunction(originalCore.engineProfile(),i-1))==0)e.add("Falta saída de ignição do cilindro "+i);}
        boolean auxiliary=channel(Group.ANALOG,3)>0||channel(Group.POWER,16)>0||channel(Group.POWER,17)>0;
        if(auxiliary&&!originalAux.configured())e.add("Prepara e guarda a calibração em Ralenti e turbo antes de atribuir TPS ou válvulas");
        boolean features=channel(Group.TEMPERATURE,2)>0||channel(Group.ANALOG,4)>0||channel(Group.ANALOG,5)>0||channel(Group.ANALOG,6)>0;
        if(features&&!new FeatureConfiguration(originalAux.bytes()).configured())e.add("Prepara e guarda a referência em Combustível e proteções antes de atribuir IAT, tensão, etanol ou launch");
        var core=new IoConfiguration(coreBytes());e.addAll(core.errors());e.addAll(new AuxConfiguration(auxBytes()).errors(core.engineProfile()));
        return List.copyOf(e.stream().distinct().toList());
    }
    private void requireValid(){var e=errors();if(!e.isEmpty())throw new IllegalArgumentException(String.join(" · ",e));}
    public IoConfiguration core(){requireValid();return new IoConfiguration(coreBytes());}
    public AuxConfiguration auxiliary(){requireValid();return new AuxConfiguration(auxBytes());}
    public List<String> changes(){var before=new IoAssignments(originalCore,originalAux);var out=new ArrayList<String>();for(var g:Group.values())for(int i=0;i<g.count;i++)if(function(g,i)!=before.function(g,i)||(before.overlaps.get(g).contains(i)&&!overlaps.get(g).contains(i)))out.add(terminal(g,i)+": "+(before.overlaps.get(g).contains(i)?"funções sobrepostas":before.functionName(g,before.function(g,i)))+" → "+functionName(g,function(g,i)));return List.copyOf(out);}
}
