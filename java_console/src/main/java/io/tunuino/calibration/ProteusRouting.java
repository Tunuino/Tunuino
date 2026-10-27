/* Copyright (c) 2026 Tunuino. Connector contacts for the Proteus routing profile. */
package io.tunuino.calibration;
public final class ProteusRouting {
    private ProteusRouting(){}
    private static final int[] ANALOG={13,24,14,25,15,26,16,27,17,28,18};
    private static final int[] TEMPERATURE={19,30,20,31};
    private static final int[] POWER={3,15,4,16,5,6,7,19,8,20,9,21,10,11,12,23};
    private static final int[] IGNITION={35,34,22,33,32,31,30,29,28,27,26,25};
    public static int contact(IoAssignments.Group group,int channel){int[] pins=switch(group){case ANALOG->ANALOG;case TEMPERATURE->TEMPERATURE;case POWER->POWER;case IGNITION->IGNITION;};if(channel<1||channel>pins.length)throw new IllegalArgumentException("Ligação fora dos limites");return pins[channel-1];}
    public static String connection(IoAssignments.Group group,int channel){return channel==0?"Desativada":(group==IoAssignments.Group.ANALOG||group==IoAssignments.Group.TEMPERATURE?"Branco 35":"Preto 35")+" · contacto "+contact(group,channel);}
    public static String shortConnection(IoAssignments.Group group,int channel){return channel==0?"Desativada":(group==IoAssignments.Group.ANALOG||group==IoAssignments.Group.TEMPERATURE?"Branco 35/":"Preto 35/")+contact(group,channel);}
    public record Recommendation(int channel,boolean conventional){public String label(){return conventional?"Recomendada":"Proposta";}}
    public static Recommendation recommendation(IoAssignments.Group group,int function){
        int channel=switch(group){
            case ANALOG->switch(function){case 1->1;case 2->10;case 3->2;case 5->4;case 6->6;default->0;};
            case TEMPERATURE->function==1?3:function==2?2:0;
            case POWER->function>=1&&function<=12?function:switch(function){case 13->10;case 14->11;case 15->12;case 16->13;case 17->14;default->0;};
            case IGNITION->function>=1&&function<=12?function:0;
        };
        return new Recommendation(channel,group==IoAssignments.Group.IGNITION||group==IoAssignments.Group.TEMPERATURE
            ||group==IoAssignments.Group.ANALOG&&function<=3||group==IoAssignments.Group.POWER&&function<=15);
    }
}
