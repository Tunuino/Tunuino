/* Copyright (c) 2026 Tunuino. Short routing choices; full selection is explicit. */
package io.tunuino.console;
import io.tunuino.calibration.*;
import java.util.*;
import java.util.function.*;
import javafx.scene.Node;
import javafx.scene.control.*;
import javafx.scene.layout.HBox;
import javafx.util.StringConverter;
final class FunctionChoices {
    private FunctionChoices(){}
    static boolean updating(ComboBox<?> choice){Object flag=choice.getProperties().get("routing-rebuilding");return flag instanceof boolean[] b&&b[0];}
    static <T> Node control(ComboBox<T> choice){Button button=(Button)choice.getProperties().get("routing-customize");if(choice.getId()!=null)button.setId(choice.getId()+"-customize");return new HBox(8,choice,button);}
    static <T> void choose(ComboBox<T> choice,IoAssignments.Group group,ProteusRouting.Recommendation recommended,ToIntFunction<T> number,Consumer<T> raw){
        var all=List.copyOf(choice.getItems());boolean[] changing={false};choice.getProperties().put("routing-rebuilding",changing);
        Function<T,String> text=n->{if(n==null)return "Escolher ligação";int ch;try{ch=number.applyAsInt(n);}catch(RuntimeException e){return "Escolher ligação";}if(ch==0)return "Desativada";return (ch==recommended.channel()?recommended.label():"Atual")+" · "+ProteusRouting.shortConnection(group,ch);};
        choice.getStyleClass().add("routing-choice");
        choice.setEditable(false);choice.setPrefWidth(245);choice.setConverter(new StringConverter<>(){
            @Override public String toString(T n){return text.apply(n);}
            @Override public T fromString(String s){throw new UnsupportedOperationException("Escolhe uma ligação da lista");}
        });
        Consumer<T> refresh=value->{if(changing[0])return;changing[0]=true;try{
            var shortList=new ArrayList<T>();for(T n:all)if(number.applyAsInt(n)==0||recommended.channel()>0&&number.applyAsInt(n)==recommended.channel())shortList.add(n);
            if(value!=null&&all.contains(value)&&!shortList.contains(value))shortList.add(value);
            choice.getItems().setAll(shortList);choice.setValue(value);raw.accept(value);
        }finally{changing[0]=false;}};
        choice.valueProperty().addListener((p,a,b)->refresh.accept(b));
        var button=new Button("Personalizar…");button.disableProperty().bind(choice.disabledProperty());choice.getProperties().put("routing-customize",button);
        button.setOnAction(e->{var dialog=new Dialog<T>();dialog.setTitle("Ligação personalizada");dialog.setHeaderText("Escolhe o contacto que corresponde à cablagem instalada");if(choice.getScene()!=null)dialog.initOwner(choice.getScene().getWindow());
            var list=new ComboBox<T>();list.getItems().setAll(all);list.setPrefWidth(350);list.setConverter(new StringConverter<>(){
                @Override public String toString(T n){return n==null?"Escolher contacto":ProteusRouting.connection(group,number.applyAsInt(n));}
                @Override public T fromString(String s){throw new UnsupportedOperationException();}
            });list.setValue(choice.getValue());dialog.getDialogPane().setContent(list);dialog.getDialogPane().getButtonTypes().addAll(ButtonType.OK,ButtonType.CANCEL);dialog.setResultConverter(b->b==ButtonType.OK?list.getValue():null);dialog.showAndWait().ifPresent(choice::setValue);
        });refresh.accept(choice.getValue());
    }
    static void integers(ComboBox<Integer> choice,IoAssignments.Group group,int function){choose(choice,group,ProteusRouting.recommendation(group,function),n->n,n->{});}
    static void numericStrings(ComboBox<String> choice,IoAssignments.Group group,int function){choose(choice,group,ProteusRouting.recommendation(group,function),Integer::parseInt,n->choice.getEditor().setText(n==null?"":n));}
}
