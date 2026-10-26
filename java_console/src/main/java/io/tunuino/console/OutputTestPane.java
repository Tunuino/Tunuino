/* Copyright (c) 2026 Tunuino. */
package io.tunuino.console;

import io.tunuino.protocol.EngineOutputClient;
import java.util.List;
import java.util.function.Consumer;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.stage.Stage;

/** Dedicated, one-shot actuator tests. Selecting a channel or preset never sends a command. */
final class OutputTestPane extends VBox {
    private final ComboBox<Integer> injector = channels(16), ignition = channels(12);
    private final ComboBox<Integer> injectorUs = durations(1000,2000,3000,5000,10000,20000);
    private final ComboBox<Integer> ignitionUs = durations(1000,1500,2000,2500,3000,4000,5000);
    private final ComboBox<Integer> auxiliaryMs = durations(100,250,500,800);
    private final ComboBox<String> auxiliary = new ComboBox<>();
    private final Button read = new Button("Ler estado"), stop = new Button("PARAR TODAS AS SAÍDAS");
    private final Button inject = new Button("Testar injetor uma vez"), spark = new Button("Testar ignição uma vez"), relay = new Button("Testar relé uma vez");
    private final Label state = new Label(), routing = new Label("Envia o perfil do motor para obter as saídas auxiliares atribuídas.");
    private byte[] profile;
    private final Consumer<ConfigurationPane.Operation<EngineOutputClient.State>> execute;
    private final Stage owner;
    OutputTestPane(Stage owner, Consumer<ConfigurationPane.Operation<EngineOutputClient.State>> execute) {
        this.owner=owner;this.execute=execute;setSpacing(16);setPadding(new javafx.geometry.Insets(0,24,24,24));setId("output-test-pane");
        read.setId("output-test-read");stop.setId("output-test-stop");stop.getStyleClass().add("danger");
        inject.setId("test-injector-start");spark.setId("test-ignition-start");relay.setId("test-aux-start");
        injector.setId("test-injector-channel");ignition.setId("test-ignition-channel");
        injectorUs.setId("test-injector-duration");ignitionUs.setId("test-ignition-duration");auxiliaryMs.setId("test-aux-duration");
        auxiliary.setId("test-aux-function");auxiliary.getItems().addAll("Bomba de combustível", "Ventoinha", "Relé principal");
        auxiliary.getSelectionModel().selectFirst();auxiliary.setOnAction(e->route());
        routing.setWrapText(true);state.setWrapText(true);state.setId("output-test-state");
        getChildren().add(ConsoleShell.card("TESTES DE ATUADORES", "Motor parado e controlo desarmado. Cada comando termina automaticamente; escolher uma opção não liga nenhuma saída.",
            ConsoleShell.actions(read,stop),state));
        TabPane tabs=new TabPane();tabs.setId("output-test-tabs");tabs.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
        tabs.getTabs().addAll(tab("Injetores", ConsoleShell.card("INJETORES · LOW-SIDE", "Um pulso de 1 a 20 ms na saída LS selecionada. Confirma a cablagem do injetor.",
                ConsoleShell.actions(new Label("Saída LS"),injector,new Label("Pulso (µs)"),injectorUs),inject)),
            tab("Ignição", ConsoleShell.card("IGNIÇÃO · SINAL LÓGICO", "Um pulso de carga até 5 ms numa saída IGN. A saída comanda a etapa externa de ignição.",
                ConsoleShell.actions(new Label("Saída IGN"),ignition,new Label("Dwell (µs)"),ignitionUs),spark)),
            tab("Relés e bomba", ConsoleShell.card("RELÉS E BOMBA", "Ativação temporizada de 100 a 800 ms, apenas nas saídas auxiliares atribuídas no perfil enviado à ECU.",
                ConsoleShell.actions(auxiliary,new Label("Duração (ms)"),auxiliaryMs),routing,relay)));
        getChildren().add(tabs);
        read.setOnAction(e->execute.accept((s,d)->EngineOutputClient.read(s)));
        stop.setOnAction(e->execute.accept((s,d)->EngineOutputClient.stop(s)));
        inject.setOnAction(e->confirm("LS "+injector.getValue(),injectorUs.getValue()+" µs",(s,d)->EngineOutputClient.pulse(s,injector.getValue()-1,injectorUs.getValue())));
        spark.setOnAction(e->confirm("IGN "+ignition.getValue(),ignitionUs.getValue()+" µs",(s,d)->EngineOutputClient.pulse(s,15+ignition.getValue(),ignitionUs.getValue())));
        relay.setOnAction(e->{int channel=auxChannel();if(channel>=0)confirm(auxiliary.getValue()+" · LS "+(channel+1),auxiliaryMs.getValue()+" ms",
            (s,d)->EngineOutputClient.auxiliaryTest(s,channel,auxiliaryMs.getValue()));});
        update(false,false,null,null,"Sem estado de saídas recebido da ECU.");
    }
    private static ComboBox<Integer> channels(int count){ComboBox<Integer> c=new ComboBox<>();for(int i=1;i<=count;i++)c.getItems().add(i);c.getSelectionModel().selectFirst();return c;}
    private static ComboBox<Integer> durations(Integer... values){ComboBox<Integer> c=new ComboBox<>();c.getItems().addAll(values);c.getSelectionModel().selectFirst();return c;}
    private static Tab tab(String title,javafx.scene.Node node){return new Tab(title,node);}
    private void confirm(String channel,String time,ConfigurationPane.Operation<EngineOutputClient.State> command){
        Alert alert=ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,"Ativar "+channel+" durante "+time+"?",ButtonType.OK,ButtonType.CANCEL);
        alert.initOwner(owner);alert.setHeaderText("Teste físico da carga ligada");
        if(alert.showAndWait().orElse(ButtonType.CANCEL)==ButtonType.OK)execute.accept(command);
    }
    private int auxChannel(){if(profile==null)return -1;return (profile[22+auxiliary.getSelectionModel().getSelectedIndex()]&255)-1;}
    private void route(){int ch=auxChannel();routing.setText(ch<0?"Função sem saída atribuída no perfil enviado à ECU.":auxiliary.getValue()+" → LS "+(ch+1));}
    void update(boolean available,boolean busy,EngineOutputClient.State value,byte[] uploaded,String description){
        profile=value!=null&&value.configured()&&uploaded!=null?uploaded.clone():null;route();state.setText(description);
        boolean idle=available&&!busy,ready=idle&&value!=null&&value.timerReady()&&!value.armed();
        read.setDisable(!idle);stop.setDisable(!idle);
        inject.setDisable(!ready);spark.setDisable(!ready);relay.setDisable(!ready||auxChannel()<0);
        for(Control c:List.of(injector,ignition,injectorUs,ignitionUs,auxiliary,auxiliaryMs))c.setDisable(!idle||value!=null&&value.armed());
        // Changing the auxiliary selection must preserve the latest availability gate.
        auxiliary.setOnAction(e->{route();relay.setDisable(!ready||auxChannel()<0);});
    }
}
