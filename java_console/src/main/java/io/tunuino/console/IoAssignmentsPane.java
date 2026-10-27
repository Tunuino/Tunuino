/* Copyright (c) 2026 Tunuino. Select functions by name for each ECU terminal. */
package io.tunuino.console;
import io.tunuino.calibration.*;
import io.tunuino.protocol.*;
import java.util.*;
import java.util.function.Consumer;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.stage.Stage;
final class IoAssignmentsPane extends VBox {
    private final ConfigurationPane.Access access;private final Consumer<EngineOutputClient.State> onState;
    private final Button read=new Button("Ler funções da ECU"),save=new Button("Guardar atribuições na ECU"),reset=new Button("Repor edição lida");
    private final Label state=new Label("Lê a ECU para obter as funções guardadas e o perfil de cilindros."),validation=new Label();
    private final EnumMap<IoAssignments.Group,List<ComboBox<IoAssignments.Function>>> choices=new EnumMap<>(IoAssignments.Group.class);
    private IoAssignments model=new IoAssignments(IoConfiguration.localDefaults(),new AuxConfiguration(new byte[256]));
    private IoAssignmentClient.Snapshot snapshot;private DeviceIdentity identity;private boolean available,busy,armed,filling;
    IoAssignmentsPane(Stage owner,ConfigurationPane.Access access,Consumer<EngineOutputClient.State> onState){
        this.access=access;this.onState=onState;setSpacing(14);setPadding(new javafx.geometry.Insets(0,24,24,24));setId("io-assignments-pane");
        read.setId("io-functions-read");save.setId("io-functions-save");reset.setId("io-functions-reset");state.setId("io-functions-state");validation.setId("io-functions-validation");state.setWrapText(true);validation.setWrapText(true);
        getChildren().add(ConsoleShell.card("FUNÇÕES DE ENTRADAS E SAÍDAS","Escolhe a função pelo nome. Cada lista contém apenas funções compatíveis com essa ligação. Selecionar prepara a edição; Guardar exige saídas desarmadas e não liga os atuadores.",ConsoleShell.actions(read,save,reset),state,validation));
        TabPane tabs=new TabPane();tabs.setId("io-functions-tabs");tabs.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
        for(var group:IoAssignments.Group.values()){
            GridPane grid=new GridPane();grid.setHgap(24);grid.setVgap(10);var column=new ColumnConstraints();column.setPercentWidth(42);grid.getColumnConstraints().addAll(column,new ColumnConstraints());
            grid.addRow(0,new Label("Ligação da ECU"),new Label("Função selecionada"));var controls=new ArrayList<ComboBox<IoAssignments.Function>>();choices.put(group,controls);
            for(int i=0;i<group.count;i++){
                final int index=i;var label=new Label(IoAssignments.terminal(group,i));label.setTooltip(new Tooltip("Terminal no conector da ECU: "+IoAssignments.connector(group,i)));
                var c=new ComboBox<IoAssignments.Function>();c.setId("io-function-"+group.name()+"-"+i);c.setPrefWidth(335);c.setMaxWidth(Double.MAX_VALUE);c.setOnAction(e->{if(!filling&&c.getValue()!=null){model=model.withFunction(group,index,c.getValue().id());fill();state.setText("Atribuições alteradas apenas na edição. Ao mover uma função, a ligação anterior fica sem essa função.");}});controls.add(c);grid.addRow(i+1,label,c);
            }
            var scroll=new ScrollPane(ConsoleShell.card(group.label.toUpperCase(Locale.ROOT),group==IoAssignments.Group.POWER||group==IoAssignments.Group.IGNITION?"As funções por cilindro seguem o perfil guardado em Motor e saídas. Todas as funções necessárias têm de continuar atribuídas. Confirma a cablagem antes de guardar.":"Selecionar uma função não calibra o sensor. Configura os pontos medidos em Sensores e entradas, Ralenti e turbo ou Combustível e proteções.",grid));scroll.setFitToWidth(true);scroll.setPrefViewportHeight(520);tabs.getTabs().add(new Tab(group.label,scroll));
        }
        getChildren().addAll(tabs,ConsoleShell.card("ENTRADAS DEDICADAS","As entradas de sincronismo têm funções fixas nesta placa.",new Label("Cambota → sensor de rotação e roda fónica"),new Label("Árvore de cames → sensor de fase do motor"),new Label("Entrada de alimentação dedicada → leitura ADC da alimentação dividida")));
        read.setOnAction(e->run((s,d)->IoAssignmentClient.read(s,d,java.time.Duration.ofSeconds(20)),false));
        reset.setOnAction(e->{if(snapshot!=null){model=snapshot.assignments();fill();state.setText("Edição reposta para a configuração lida. A ECU não foi alterada.");}});
        save.setOnAction(e->{try{var desired=model;if(!desired.errors().isEmpty())throw new IllegalArgumentException(String.join(" · ",desired.errors()));
            var changes=desired.changes();if(changes.isEmpty())return;var alert=ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,"As funções serão guardadas para a próxima ativação do motor. Confirma as ligações físicas.\n\n"+String.join("\n",changes),ButtonType.OK,ButtonType.CANCEL);alert.setHeaderText("Guardar atribuições de funções");alert.initOwner(owner);if(alert.showAndWait().orElse(ButtonType.CANCEL)!=ButtonType.OK)return;
            var expected=snapshot;run((s,d)->IoAssignmentClient.apply(s,d,expected,desired,java.time.Duration.ofSeconds(30)),true);
        }catch(IllegalArgumentException x){state.setText(x.getMessage());}});fill();
    }
    private void run(ConfigurationPane.Operation<IoAssignmentClient.Snapshot> operation,boolean saved){if(!available||busy)return;busy=true;controls();access.run(operation,v->{busy=false;snapshot=v;armed=v.engine().armed();model=v.assignments();fill();state.setText(saved?"Funções guardadas e confirmadas por leitura da ECU. As saídas continuam desarmadas.":"Funções lidas da ECU · "+(model.cylinders()>0?model.cylinders()+" cilindros":"configura primeiro o perfil do motor para atribuir injetores e ignição"));onState.accept(v.engine());controls();},x->{busy=false;state.setText(x.getMessage());controls();});}
    private void fill(){filling=true;try{for(var group:choices.keySet())for(int i=0;i<group.count;i++){var c=choices.get(group).get(i);c.getItems().setAll(model.functions(group));int selected=model.function(group,i);c.setValue(c.getItems().stream().filter(f->f.id()==selected).findFirst().orElse(c.getItems().get(0)));}}finally{filling=false;}controls();}
    IoAssignments candidate(){return model;}
    void availability(DeviceIdentity d,boolean connected,boolean blocked,boolean engineArmed){if(!Objects.equals(identity,d)||!connected){identity=d;snapshot=null;}available=connected&&d!=null&&!blocked;armed=engineArmed;controls();}
    private void controls(){var errors=model.errors();boolean valid=errors.isEmpty();validation.setText(valid?"Atribuições locais compatíveis; a ECU também valida ao guardar.":String.join(" · ",errors));read.setDisable(!available||busy);save.setDisable(!available||busy||armed||snapshot==null||snapshot.core().validation().dirty()||!valid||model.changes().isEmpty());reset.setDisable(busy||armed||snapshot==null);choices.values().forEach(list->list.forEach(c->c.setDisable(busy||armed)));}
}
