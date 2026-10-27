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
    private final Button recommendations=new Button("Usar ligações recomendadas");
    private final Label state=new Label("Lê a ECU para obter as funções guardadas e o perfil de cilindros."),validation=new Label();
    private final EnumMap<IoAssignments.Group,List<ComboBox<IoAssignments.Function>>> choices=new EnumMap<>(IoAssignments.Group.class);
    private record Quick(IoAssignments.Group group,int function){}
    private final Map<Quick,ComboBox<Integer>> quick=new LinkedHashMap<>();
    private final TabPane quickTabs=new TabPane();private int quickCylinders=-1;
    private IoAssignments model=new IoAssignments(IoConfiguration.localDefaults(),new AuxConfiguration(new byte[256]));
    private IoAssignmentClient.Snapshot snapshot;private DeviceIdentity identity;private boolean available,busy,armed,filling;
    IoAssignmentsPane(Stage owner,ConfigurationPane.Access access,Consumer<EngineOutputClient.State> onState){
        this.access=access;this.onState=onState;setSpacing(14);setPadding(new javafx.geometry.Insets(0,24,24,24));setId("io-assignments-pane");
        read.setId("io-functions-read");save.setId("io-functions-save");reset.setId("io-functions-reset");state.setId("io-functions-state");validation.setId("io-functions-validation");state.setWrapText(true);validation.setWrapText(true);
        recommendations.setId("io-functions-recommend");
        recommendations.setOnAction(e->recommend());
        getChildren().add(ConsoleShell.card("LIGAÇÕES RÁPIDAS — PROTEUS","Uma função, uma ligação recomendada. Mantém a ligação atual ou escolhe Personalizar se a tua cablagem for diferente. Alterações só são enviadas ao guardar.",ConsoleShell.actions(read,save,reset,recommendations),state,validation));
        quickTabs.setId("io-quick-tabs");quickTabs.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);getChildren().add(quickTabs);
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
        TitledPane advanced=new TitledPane("Avançado — atribuição manual por terminal",tabs);advanced.setId("io-functions-advanced");advanced.setExpanded(false);
        getChildren().addAll(advanced,ConsoleShell.card("SINCRONISMO","Ligações dedicadas nesta versão.",new Label("Cambota → sensor de rotação e roda fónica"),new Label("Árvore de cames → sensor de fase")));
        read.setOnAction(e->run((s,d)->IoAssignmentClient.read(s,d,java.time.Duration.ofSeconds(20)),false));
        reset.setOnAction(e->{if(snapshot!=null){model=snapshot.assignments();fill();state.setText("Edição reposta para a configuração lida. A ECU não foi alterada.");}});
        save.setOnAction(e->{try{var desired=model;if(!desired.errors().isEmpty())throw new IllegalArgumentException(String.join(" · ",desired.errors()));
            var changes=desired.changes();if(changes.isEmpty())return;var alert=ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,"As funções serão guardadas para a próxima ativação do motor. Confirma as ligações físicas.\n\n"+String.join("\n",changes),ButtonType.OK,ButtonType.CANCEL);alert.setHeaderText("Guardar atribuições de funções");alert.initOwner(owner);if(alert.showAndWait().orElse(ButtonType.CANCEL)!=ButtonType.OK)return;
            var expected=snapshot;run((s,d)->IoAssignmentClient.apply(s,d,expected,desired,java.time.Duration.ofSeconds(30)),true);
        }catch(IllegalArgumentException x){state.setText(x.getMessage());}});fill();
    }
    private void run(ConfigurationPane.Operation<IoAssignmentClient.Snapshot> operation,boolean saved){if(!available||busy)return;busy=true;controls();access.run(operation,v->{busy=false;snapshot=v;armed=v.engine().armed();model=v.assignments();fill();state.setText(saved?"Funções guardadas e confirmadas por leitura da ECU. As saídas continuam desarmadas.":"Funções lidas da ECU · "+(model.cylinders()>0?model.cylinders()+" cilindros":"configura primeiro o perfil do motor para atribuir injetores e ignição"));onState.accept(v.engine());controls();},x->{busy=false;state.setText(x.getMessage());controls();});}
    private void fill(){filling=true;try{if(quickCylinders!=model.cylinders())buildQuick();for(var entry:quick.entrySet())entry.getValue().setValue(channel(entry.getKey()));for(var group:choices.keySet())for(int i=0;i<group.count;i++){var c=choices.get(group).get(i);c.getItems().setAll(model.functions(group));int selected=model.function(group,i);c.setValue(c.getItems().stream().filter(f->f.id()==selected).findFirst().orElse(c.getItems().get(0)));}}finally{filling=false;}controls();}
    private int channel(Quick key){for(int i=0;i<key.group().count;i++)if(model.function(key.group(),i)==key.function())return i+1;return 0;}
    private void row(GridPane grid,IoAssignments.Group group,int function){
        Quick key=new Quick(group,function);ComboBox<Integer> c=new ComboBox<>();c.setId("io-quick-"+group.name()+"-"+function);for(int i=0;i<=group.count;i++)c.getItems().add(i);c.setValue(0);FunctionChoices.integers(c,group,function);quick.put(key,c);
        c.setOnAction(e->{if(filling||FunctionChoices.updating(c)||c.getValue()==null)return;int chosen=c.getValue(),old=channel(key);
            if(chosen>0){int owner=model.function(group,chosen-1);if(owner!=0&&owner!=function){state.setText(ProteusRouting.connection(group,chosen)+" já está atribuído a "+model.functionName(group,owner)+". Mantive as ligações anteriores.");fill();return;}}
            var next=model;if(old>0)next=next.withFunction(group,old-1,0);if(chosen>0)next=next.withFunction(group,chosen-1,function);model=next;fill();state.setText("Ligação preparada na edição. Guarda para aplicar à ECU.");
        });var name=new Label(model.functionName(group,function));name.setWrapText(true);grid.addRow(grid.getRowCount(),name,FunctionChoices.control(c));
    }
    private GridPane grid(){GridPane g=new GridPane();g.setHgap(24);g.setVgap(10);return g;}
    private void buildQuick(){
        quickCylinders=model.cylinders();int selected=quickTabs.getSelectionModel().getSelectedIndex();quick.clear();quickTabs.getTabs().clear();
        var sensors=grid();for(int f:new int[]{1,3})row(sensors,IoAssignments.Group.ANALOG,f);row(sensors,IoAssignments.Group.TEMPERATURE,1);row(sensors,IoAssignments.Group.TEMPERATURE,2);row(sensors,IoAssignments.Group.ANALOG,2);
        var extra=grid();for(int f:new int[]{4,5,6})row(extra,IoAssignments.Group.ANALOG,f);TitledPane others=new TitledPane("Outros sensores",extra);others.setExpanded(false);
        quickTabs.getTabs().add(new Tab("Sensores",ConsoleShell.card("SENSORES","As ligações habituais da Proteus aparecem primeiro. Selecionar não substitui a calibração do sensor.",sensors,others)));
        var cylinders=grid();for(int i=1;i<=model.cylinders();i++){row(cylinders,IoAssignments.Group.POWER,i);row(cylinders,IoAssignments.Group.IGNITION,i);}
        quickTabs.getTabs().add(new Tab("Injeção e ignição",ConsoleShell.card("CILINDROS",model.cylinders()==0?"Configura e guarda o perfil em Motor e saídas para apresentar os cilindros.":"Cada cilindro usa a ligação do mesmo número no pinout. A ordem de ignição mantém-se no perfil do motor.",cylinders)));
        var valves=grid();for(int f:new int[]{13,14,15,16,17})row(valves,IoAssignments.Group.POWER,f);
        quickTabs.getTabs().add(new Tab("Relés e válvulas",ConsoleShell.card("BOMBA, VENTOINHA E AUXILIARES","A ligação recomendada é mostrada por contacto do conector. Proposta identifica uma escolha Tunuino para funções sem ligação habitual fixa.",valves)));
        quickTabs.getSelectionModel().select(Math.max(0,Math.min(2,selected)));
    }
    private void recommend(){if(busy||armed)return;try{
        var selected=new ArrayList<Quick>();for(var key:quick.keySet())if(channel(key)>0&&ProteusRouting.recommendation(key.group(),key.function()).channel()>0)selected.add(key);
        var next=model;for(var key:selected){int old=channel(key);next=next.withFunction(key.group(),old-1,0);}
        for(var key:selected){int target=ProteusRouting.recommendation(key.group(),key.function()).channel();int owner=next.function(key.group(),target-1);if(owner!=0)throw new IllegalArgumentException("As ligações recomendadas entram em conflito em "+ProteusRouting.connection(key.group(),target)+". Usa Personalizar para esse caso.");next=next.withFunction(key.group(),target-1,key.function());}
        model=next;fill();state.setText("Ligações recomendadas preparadas para as funções já atribuídas. A ECU não foi alterada.");
    }catch(IllegalArgumentException x){state.setText(x.getMessage());}}
    IoAssignments candidate(){return model;}
    void availability(DeviceIdentity d,boolean connected,boolean blocked,boolean engineArmed){if(!Objects.equals(identity,d)||!connected){identity=d;snapshot=null;}available=connected&&d!=null&&!blocked;armed=engineArmed;controls();}
    private void controls(){var errors=model.errors();boolean valid=errors.isEmpty();validation.setText(valid?"Ligações compatíveis.":String.join(" · ",errors));read.setDisable(!available||busy);save.setDisable(!available||busy||armed||snapshot==null||snapshot.core().validation().dirty()||!valid||model.changes().isEmpty());reset.setDisable(busy||armed||snapshot==null);recommendations.setDisable(busy||armed);choices.values().forEach(list->list.forEach(c->c.setDisable(busy||armed)));quick.values().forEach(c->c.setDisable(busy||armed));}
}
