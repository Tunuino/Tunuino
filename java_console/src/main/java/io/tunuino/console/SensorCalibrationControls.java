/* Copyright (c) 2026 Tunuino. Shared local-only NTC calibration assistant. */
package io.tunuino.console;
import io.tunuino.calibration.SensorCalibration;
import java.util.function.Consumer;
import javafx.scene.control.*;
import javafx.scene.layout.*;

final class SensorCalibrationControls {
    private SensorCalibrationControls() {}
    static TitledPane ntc(String id, Consumer<SensorCalibration.Point[]> apply, javafx.beans.value.ObservableBooleanValue blocked) {
        var model=new ComboBox<String>();model.getItems().addAll("NTC 2,2 kΩ · B3950","NTC 10 kΩ · B3950","Personalizado");model.getSelectionModel().selectFirst();
        var r25=new TextField("2200");var beta=new TextField("3950");var pullup=new TextField("2490");
        var low=new TextField("-20");var middle=new TextField("40");var high=new TextField("120");
        model.setId(id+"-model");pullup.setId(id+"-pullup");
        model.setOnAction(e->{if(model.getSelectionModel().getSelectedIndex()<2){r25.setText(model.getSelectionModel().getSelectedIndex()==0?"2200":"10000");beta.setText("3950");}});
        var g=new GridPane();g.setHgap(12);g.setVgap(8);
        g.addRow(0,new Label("Modelo do sensor"),model);g.addRow(1,new Label("Resistência a 25 °C (Ω)"),r25);
        g.addRow(2,new Label("Beta do sensor (K)"),beta);g.addRow(3,new Label("Pull-up real da entrada (Ω)"),pullup);
        g.addRow(4,new Label("Temperaturas dos três pontos (°C)"),new HBox(8,low,middle,high));
        for(var f:java.util.List.of(r25,beta,pullup,low,middle,high))f.setPrefColumnCount(6);
        var note=new Label("Modelo NTC para sensor à massa e pull-up à referência ADC. Confirma a curva e o pull-up da instalação. O firmware interpola os três pontos gerados.");note.setWrapText(true);
        var result=new Label();result.setWrapText(true);
        var use=new Button("Gerar curva na edição");use.setId(id+"-apply");use.setOnAction(e->{try{
            var points=SensorCalibration.ntc(n(r25),n(beta),n(pullup),n(low),n(middle),n(high));apply.accept(points);result.setText("Três pontos preparados. Revê a curva e guarda na ECU para aplicar.");
        }catch(IllegalArgumentException x){result.setText(x.getMessage());}});
        VBox content=new VBox(10,note,g,use,result);content.disableProperty().bind(blocked);
        var pane=new TitledPane("Calibrar sensor NTC por modelo / resistência",content);pane.setExpanded(false);pane.setAnimated(false);pane.setId(id);return pane;
    }
    private static double n(TextField f){try{return Double.parseDouble(f.getText().trim().replace(',','.'));}catch(RuntimeException e){throw new IllegalArgumentException("Introduz os valores do sensor e do circuito");}}
}
