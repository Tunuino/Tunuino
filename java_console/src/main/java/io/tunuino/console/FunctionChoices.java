/* Copyright (c) 2026 Tunuino. Functional names with stable wire channel values. */
package io.tunuino.console;
import io.tunuino.calibration.IoAssignments;
import javafx.scene.control.ComboBox;
import javafx.util.StringConverter;
final class FunctionChoices {
    private FunctionChoices(){}
    private static String label(String function,IoAssignments.Group group,int channel){return channel==0?"Sem atribuição":function+" · "+IoAssignments.terminal(group,channel-1).toLowerCase(java.util.Locale.ROOT);}
    static void integers(ComboBox<Integer> choice,String function,IoAssignments.Group group){
        choice.setPrefWidth(310);choice.setConverter(new StringConverter<>(){
            @Override public String toString(Integer n){return n==null?"Selecionar função":label(function,group,n);}
            @Override public Integer fromString(String s){throw new UnsupportedOperationException("Seleciona uma função da lista");}
        });
    }
    static void numericStrings(ComboBox<String> choice,String function,IoAssignments.Group group){
        choice.setEditable(false);choice.setPrefWidth(group==IoAssignments.Group.IGNITION?260:300);
        choice.setConverter(new StringConverter<>(){
            @Override public String toString(String n){return n==null||n.isBlank()?"Selecionar função":label(function,group,Integer.parseInt(n));}
            @Override public String fromString(String s){throw new UnsupportedOperationException("Seleciona uma função da lista");}
        });
        // The existing configuration encoder retains the canonical numeric value;
        // the hidden editor is never used as a user-facing pin selector.
        choice.valueProperty().addListener((p,a,b)->choice.getEditor().setText(b==null?"":b));
    }
}
