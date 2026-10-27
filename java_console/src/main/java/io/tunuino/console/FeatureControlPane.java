/* Copyright (c) 2026 Tunuino. Optional fuel strategies and engine limits. */
package io.tunuino.console;
import io.tunuino.calibration.*;
import io.tunuino.protocol.*;
import java.util.*;
import java.util.function.Consumer;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.stage.*;
final class FeatureControlPane extends VBox {
    private final ConfigurationPane.Access access;private final Consumer<EngineOutputClient.State> onState;
    private final EnumMap<FeatureConfiguration.Field,ComboBox<String>> fields=new EnumMap<>(FeatureConfiguration.Field.class);
    private final EnumMap<FeatureConfiguration.Option,CheckBox> options=new EnumMap<>(FeatureConfiguration.Option.class);
    private final Button read=new Button("Ler configuração e estado"),save=new Button("Guardar funções na ECU"),base=new Button("Aplicar referência desativada"),backup=new Button("Guardar cópia .tnaux"),open=new Button("Abrir cópia .tnaux");
    private final ComboBox<String> reference=new ComboBox<>();
    private final Label result=new Label("Sem configuração recebida da ECU.");
    private AuxControlClient.Snapshot snapshot;private byte[] baseline=AuxConfiguration.reference().bytes();
    private DeviceIdentity identity;private boolean available,busy,armed;
    FeatureControlPane(Stage owner,ConfigurationPane.Access access,Consumer<EngineOutputClient.State> onState){
        this.access=access;this.onState=onState;setSpacing(14);setPadding(new javafx.geometry.Insets(0,24,24,24));setId("feature-control-pane");
        read.setId("features-read");save.setId("features-save");base.setId("features-reference");result.setId("features-state");result.setWrapText(true);
        reference.getItems().addAll("Gasolina · aspirado","Gasolina · sobrealimentado");reference.getSelectionModel().select(0);
        getChildren().add(ConsoleShell.card("COMBUSTÍVEL E PROTEÇÕES","Seleciona as funções necessárias e calibra as entradas. Os valores de referência começam com todas as novas funções desativadas. Guardar exige saídas desarmadas.",ConsoleShell.actions(read,save),result));
        var tabs=new TabPane();tabs.setId("features-tabs");tabs.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
        Map<String,GridPane> grids=new LinkedHashMap<>();
        Map<String,String> help=Map.of(
            "Sensores","IAT usa uma entrada TEMP independente do líquido de refrigeração. A tensão usa uma entrada AV e deve ser calibrada em VOLTS DA BATERIA, incluindo o divisor. Mede os pontos ADC; as referências não são calibrações do sensor.",
            "Tensão","Curvas de três tensões para o tempo morto dos injetores e dwell das bobinas. Todos os tempos são apresentados em milissegundos. Usa os dados do fabricante dos componentes instalados.",
            "Lambda","Curva alvo de três pontos MAP absolutos. O combustível base deve referir-se a lambda 1; ativar o alvo enriquece por 1 / lambda alvo. A malha fechada é PI com correção limitada e condições de temperatura, RPM, TPS e atraso. Não altera o mapa VE nem faz Autotune.",
            "Transientes","Aceleração: combustível adicional por velocidade de abertura TPS, com decaimento linear. DFCO: corte com temperatura mínima, atraso e histerese de RPM. A malha fechada suspende a correção durante estes eventos.",
            "Limites","Limitador e launch cortam combustível com histerese; a ignição mantém o dwell programado. Launch exige uma entrada AV condicionada, com níveis válidos de 100 a 3995 ADC. Um sinal fora dessa faixa causa corte retido. Proteções CLT/tensão também exigem reativação explícita.",
            "Flex fuel","Requer controlador de etanol com saída analógica calibrada entre 0 e 100%. Esta entrada não aceita diretamente sensores de frequência. O firmware interpola a correção de combustível e avanço; afina para o combustível e motor usados.",
            "Cilindros","Ajustes individuais sobre o combustível efetivo, antes do tempo morto, e sobre o avanço. Confirmar medição por cilindro antes de ativar. Os limites de pulso e duty continuam a aplicar-se.");
        for(String group:List.of("Sensores","Tensão","Lambda","Transientes","Limites","Flex fuel","Cilindros")){
            GridPane grid=new GridPane();grid.setHgap(18);grid.setVgap(9);grids.put(group,grid);
            var column=new ColumnConstraints();column.setPercentWidth(65);grid.getColumnConstraints().addAll(column,new ColumnConstraints());
            var scroll=new ScrollPane(ConsoleShell.card(group.toUpperCase(Locale.ROOT),help.get(group),grid));scroll.setFitToWidth(true);scroll.setPrefViewportHeight(560);tabs.getTabs().add(new Tab(group,scroll));
        }
        for(var option:FeatureConfiguration.Option.values()){var c=new CheckBox(option.label);c.setId("feature-option-"+option.name());c.setWrapText(true);options.put(option,c);GridPane g=grids.get(option.group);g.add(c,0,g.getRowCount(),2,1);}
        for(var f:FeatureConfiguration.Field.values()){
            var c=new ComboBox<String>();c.setEditable(true);c.setPrefWidth(165);c.setId("feature-choice-"+f.name());c.getEditor().setId("feature-field-"+f.name());
            if(f.offset<76){for(int i=f.min;i<=f.max;i++)c.getItems().add(Integer.toString(i));
                String function=switch(f){case IAT_INPUT->"IAT — temperatura do ar";case VOLT_INPUT->"Tensão da bateria";case LAUNCH_INPUT->"Comando de launch control";case FLEX_INPUT->"Etanol — flex fuel";default->throw new IllegalArgumentException();};
                FunctionChoices.numericStrings(c,function,f==FeatureConfiguration.Field.IAT_INPUT?io.tunuino.calibration.IoAssignments.Group.TEMPERATURE:io.tunuino.calibration.IoAssignments.Group.ANALOG);
            }
            else if(f.decimals==3&&f.name().contains("TARGET"))c.getItems().addAll("0.85","0.9","0.95","1");
            else c.getItems().add(java.math.BigDecimal.valueOf(f.reference,f.decimals).stripTrailingZeros().toPlainString());
            fields.put(f,c);GridPane g=grids.get(f.group);var l=new Label(f.label);l.setWrapText(true);g.addRow(g.getRowCount(),l,c);
        }
        getChildren().addAll(tabs,ConsoleShell.card("REFERÊNCIAS E CÓPIAS","O ficheiro .tnaux inclui TPS, ralenti, turbo e todas estas funções. Abrir uma cópia prepara a edição; só Guardar envia à ECU. Guarda também o perfil .tncfg e os mapas .tnmap.",ConsoleShell.actions(reference,base,backup,open)));
        fill(baseline);
        base.setOnAction(e->{byte[] p=FeatureConfiguration.reference(baseline).bytes();if(reference.getSelectionModel().getSelectedIndex()==1){var b=java.nio.ByteBuffer.wrap(p);b.putShort(118,(short)25000).putShort(124,(short)850);}fill(p);result.setText("Referência aplicada à edição, com todas as novas funções desativadas. As definições TPS/ralenti/turbo foram preservadas.");});
        read.setOnAction(e->run((s,d)->new Loaded(AuxControlClient.read(s,d,java.time.Duration.ofSeconds(10)),AuxControlClient.features(s),EngineOutputClient.read(s))));
        save.setOnAction(e->{try{var candidate=candidate();if(snapshot==null)throw new IllegalArgumentException("Lê a configuração antes de guardar");
            var a=ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,"Guardar as funções selecionadas para a próxima ativação do motor? Confirma sensores, curvas e limites para o motor e componentes instalados.",ButtonType.OK,ButtonType.CANCEL);a.initOwner(owner);a.setHeaderText("Guardar combustível e proteções");if(a.showAndWait().orElse(ButtonType.CANCEL)!=ButtonType.OK)return;
            var expected=snapshot;run((s,d)->new Loaded(AuxControlClient.apply(s,d,expected,candidate,java.time.Duration.ofSeconds(25)),AuxControlClient.features(s),EngineOutputClient.read(s)));
        }catch(IllegalArgumentException x){result.setText(x.getMessage());}});
        backup.setOnAction(e->{try{var c=candidate();var f=chooser("Guardar auxiliares e funções");f.setInitialFileName("auxiliares-funcoes.tnaux");var file=f.showSaveDialog(owner);if(file!=null){c.writeNew(file.toPath());result.setText("Cópia guardada: "+file.getName());}}catch(Exception x){result.setText(x.getMessage());}});
        open.setOnAction(e->{var file=chooser("Abrir auxiliares e funções").showOpenDialog(owner);if(file!=null)try{fill(AuxConfiguration.read(file.toPath()).bytes());result.setText("Cópia aberta apenas na edição.");}catch(Exception x){result.setText(x.getMessage());}});controls();
    }
    private static FileChooser chooser(String title){var f=new FileChooser();f.setTitle(title);f.getExtensionFilters().add(new FileChooser.ExtensionFilter("Tunuino auxiliares e funções","*.tnaux"));return f;}
    private record Loaded(AuxControlClient.Snapshot snapshot,AuxControlClient.FeatureState features,EngineOutputClient.State engine){}
    private void run(ConfigurationPane.Operation<Loaded> operation){if(!available||busy)return;busy=true;controls();access.run(operation,v->{busy=false;snapshot=v.snapshot();armed=v.engine().armed();fill(v.snapshot().configuration().bytes());var f=v.features();result.setText(f.description()+" · "+v.engine().description());onState.accept(v.engine());controls();},x->{busy=false;result.setText(x.getMessage());controls();});}
    private void fill(byte[] page){baseline=(page[0]==1?page:AuxConfiguration.reference().bytes()).clone();var f=new FeatureConfiguration(baseline);if(!f.configured())f=FeatureConfiguration.reference(baseline);for(var field:fields.keySet()){String text=f.value(field);fields.get(field).setValue(text);fields.get(field).getEditor().setText(text);}for(var o:options.keySet())options.get(o).setSelected(f.enabled(o));}
    AuxConfiguration candidate(){var values=new EnumMap<FeatureConfiguration.Field,String>(FeatureConfiguration.Field.class);fields.forEach((f,c)->values.put(f,c.getEditor().getText()));var enabled=EnumSet.noneOf(FeatureConfiguration.Option.class);options.forEach((o,c)->{if(c.isSelected())enabled.add(o);});return new AuxConfiguration(new FeatureConfiguration(baseline).edited(values,enabled).bytes());}
    void availability(DeviceIdentity d,boolean connected,boolean blocked,boolean engineArmed){if(!Objects.equals(identity,d)||!connected){identity=d;snapshot=null;}available=connected&&d!=null&&!blocked;armed=engineArmed;controls();}
    private void controls(){boolean edit=!busy&&!armed;read.setDisable(!available||busy);save.setDisable(!available||busy||armed||snapshot==null);base.setDisable(!edit);reference.setDisable(!edit);open.setDisable(!edit);backup.setDisable(busy);fields.values().forEach(c->c.setDisable(!edit));options.values().forEach(c->c.setDisable(!edit));}
}
