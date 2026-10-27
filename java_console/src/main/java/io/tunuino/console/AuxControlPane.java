/* Copyright (c) 2026 Tunuino. Editable TPS, idle and boost configuration. */
package io.tunuino.console;
import io.tunuino.calibration.AuxConfiguration;
import io.tunuino.protocol.*;
import java.util.*;
import java.util.function.Consumer;
import javafx.scene.control.*;
import javafx.scene.layout.*;
import javafx.stage.*;
final class AuxControlPane extends VBox {
    private final ConfigurationPane.Access access;private final Consumer<EngineOutputClient.State> onState;
    private final EnumMap<AuxConfiguration.Field,TextField> fields=new EnumMap<>(AuxConfiguration.Field.class);
    private final EnumMap<AuxConfiguration.Field,ComboBox<String>> choices=new EnumMap<>(AuxConfiguration.Field.class);
    private final ComboBox<String> idleMode=new ComboBox<>(),boostMode=new ComboBox<>();
    private final ComboBox<Integer> idleChannel=new ComboBox<>(),boostChannel=new ComboBox<>(),tpsChannel=new ComboBox<>();
    private final Button read=new Button("Ler configuração e estado"),save=new Button("Guardar ralenti/turbo na ECU"),base=new Button("Aplicar referência à edição"),backup=new Button("Guardar cópia .tnaux"),open=new Button("Abrir cópia .tnaux");
    private final Label result=new Label("Sem configuração recebida da ECU.");
    private AuxControlClient.Snapshot snapshot;private AuxConfiguration baseline=new AuxConfiguration(new byte[256]);
    private boolean available,busy,armed;private DeviceIdentity identity;
    AuxControlPane(Stage owner,ConfigurationPane.Access access,Consumer<EngineOutputClient.State> onState){
        this.access=access;this.onState=onState;setSpacing(14);setPadding(new javafx.geometry.Insets(0,24,24,24));setId("aux-control-pane");
        read.setId("aux-read");save.setId("aux-save");base.setId("aux-reference");result.setId("aux-state");result.setWrapText(true);
        idleMode.getItems().addAll("Desativado","PWM por temperatura","PWM com controlo PI de RPM");boostMode.getItems().addAll("Desativado","Duty fixo","Controlo PI de pressão MAP");
        idleMode.setId("aux-idle-mode");boostMode.setId("aux-boost-mode");idleChannel.setId("aux-idle-channel");boostChannel.setId("aux-boost-channel");tpsChannel.setId("aux-tps-channel");
        for(int i=0;i<=16;i++){idleChannel.getItems().add(i);boostChannel.getItems().add(i);}for(int i=0;i<=11;i++)tpsChannel.getItems().add(i);
        Map<String,GridPane> grids=new LinkedHashMap<>();for(String name:List.of("TPS","Ralenti","Turbo")){GridPane g=new GridPane();g.setHgap(18);g.setVgap(9);grids.put(name,g);}
        grids.get("TPS").addRow(0,new Label("TPS: 0 desativado, 1–11 AV"),tpsChannel);
        grids.get("Ralenti").addRow(0,new Label("Modo"),idleMode);grids.get("Ralenti").addRow(1,new Label("Saída LS (0 sem atribuição)"),idleChannel);
        grids.get("Turbo").addRow(0,new Label("Modo"),boostMode);grids.get("Turbo").addRow(1,new Label("Saída LS (0 sem atribuição)"),boostChannel);
        for(var f:AuxConfiguration.Field.values()){
            String group=f.name().startsWith("TPS_")?"TPS":f.name().startsWith("BOOST_")?"Turbo":"Ralenti";
            ComboBox<String> c=new ComboBox<>();c.setEditable(true);c.setPrefWidth(175);TextField input=c.getEditor();input.setId("aux-field-"+f.name());fields.put(f,input);choices.put(f,c);
            if(f.name().contains("DUTY")||f.name().contains("BASE")||f.name().endsWith("_MAX")||f.name().endsWith("_MIN"))c.getItems().addAll("0","10","20","30","40","50");
            if(f.name().endsWith("_HZ"))c.getItems().addAll("25","50","100");
            GridPane grid=grids.get(group);grid.addRow(grid.getRowCount(),new Label(f.label),c);
        }
        getChildren().add(ConsoleShell.card("TPS, RALENTI E TURBO","As saídas devem ser independentes dos injetores, bomba, ventoinha e relé. O firmware recusa conflitos. Guardar exige motor desarmado.",ConsoleShell.actions(read,save),result));
        TabPane tabs=new TabPane();tabs.setId("aux-tabs");tabs.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
        tabs.getTabs().addAll(new Tab("TPS",ConsoleShell.card("CALIBRAÇÃO TPS","Introduz as contagens ADC medidas com borboleta fechada e totalmente aberta. A calibração pode ter sentido crescente ou decrescente. 0 significa entrada desativada.",grids.get("TPS"))),
            new Tab("Ralenti",ConsoleShell.card("RALENTI PWM","Curvas de três temperaturas para alvo RPM e duty base. Durante arranque usa o duty de arranque. Fora dos limites TPS/RPM ou com sinal inválido a válvula desliga. Esta revisão suporta válvulas PWM de dois fios.",grids.get("Ralenti"))),
            new Tab("Turbo",ConsoleShell.card("TURBO PWM","MAP em kPa ABSOLUTOS. A válvula deve produzir pressão de mola quando desligada. Confirma ligações e afina frequência, duty, alvo e ganhos. O corte de pressão desativa injeção, ignição e auxiliares até nova ativação. Os mapas base aspirados não cobrem turbo.",grids.get("Turbo"))));
        getChildren().addAll(tabs,ConsoleShell.card("REFERÊNCIAS E CÓPIAS","A referência começa com os dois controlos desativados e duty de turbo zero. Mede o TPS, escolhe saídas livres e afina os valores para as válvulas instaladas. O ficheiro .tnaux guarda esta página; guarda também o perfil .tncfg e os mapas .tnmap.",ConsoleShell.actions(base,backup,open)));
        fill(new AuxConfiguration(new byte[256]));
        base.setOnAction(e->{byte[] p=baseline.bytes();System.arraycopy(AuxConfiguration.reference().bytes(),0,p,0,68);fill(new AuxConfiguration(p));result.setText("Referência aplicada apenas à edição. Ralenti e turbo continuam desativados até configurares e guardares os modos.");});
        read.setOnAction(e->load());
        save.setOnAction(e->{try{var candidate=candidate();if(snapshot==null)throw new IllegalArgumentException("Lê a configuração da ECU antes de guardar");
            Alert a=ConsoleBranding.alert(Alert.AlertType.CONFIRMATION,"Guardar estas configurações para a próxima ativação do motor? As válvulas podem comandar ar e pressão de turbo quando o motor for ativado.",ButtonType.OK,ButtonType.CANCEL);a.initOwner(owner);a.setHeaderText("Guardar ralenti e turbo");if(a.showAndWait().orElse(ButtonType.CANCEL)!=ButtonType.OK)return;
            var expected=snapshot;run((s,d)->{var saved=AuxControlClient.apply(s,d,expected,candidate,java.time.Duration.ofSeconds(25));return new Loaded(saved,AuxControlClient.state(s),EngineOutputClient.read(s));});
        }catch(IllegalArgumentException x){result.setText(x.getMessage());}});
        backup.setOnAction(e->{try{var c=candidate();FileChooser f=new FileChooser();f.setTitle("Guardar cópia de ralenti/turbo");f.getExtensionFilters().add(new FileChooser.ExtensionFilter("Tunuino auxiliares","*.tnaux"));f.setInitialFileName("ralenti-turbo.tnaux");var file=f.showSaveDialog(owner);if(file!=null){c.writeNew(file.toPath());result.setText("Cópia guardada: "+file.getName());}}catch(Exception x){result.setText(x.getMessage());}});
        open.setOnAction(e->{FileChooser f=new FileChooser();f.setTitle("Abrir cópia de ralenti/turbo");f.getExtensionFilters().add(new FileChooser.ExtensionFilter("Tunuino auxiliares","*.tnaux"));var file=f.showOpenDialog(owner);if(file!=null)try{fill(AuxConfiguration.read(file.toPath()));result.setText("Cópia aberta apenas na edição; confirma e guarda para aplicar à ECU.");}catch(Exception x){result.setText(x.getMessage());}});controls();
    }
    private record Loaded(AuxControlClient.Snapshot snapshot,AuxControlClient.State auxiliary,EngineOutputClient.State engine){}
    private void load(){run((s,d)->new Loaded(AuxControlClient.read(s,d,java.time.Duration.ofSeconds(10)),AuxControlClient.state(s),EngineOutputClient.read(s)));}
    private void run(ConfigurationPane.Operation<Loaded> operation){if(!available||busy)return;busy=true;controls();access.run(operation,v->{busy=false;snapshot=v.snapshot();armed=v.engine().armed();fill(v.snapshot().configuration());var a=v.auxiliary();result.setText((v.snapshot().configuration().configured()?"Configuração lida":"Página de fábrica desativada; campos mostram uma referência local")+" · TPS "+(a.tpsValid()?String.format(Locale.forLanguageTag("pt-PT"),"%.2f %%",a.tps()/100.0):"indisponível")+" · ralenti "+a.idleDuty()/100.0+" % / alvo "+a.idleTarget()+" rpm · turbo "+a.boostDuty()/100.0+" % / alvo "+a.boostTarget()/100.0+" kPa absolutos · "+v.engine().description());onState.accept(v.engine());controls();},x->{busy=false;result.setText(x.getMessage());controls();});}
    private void fill(AuxConfiguration configuration){baseline=configuration;AuxConfiguration c=configuration.configured()?configuration:AuxConfiguration.reference();for(var f:fields.keySet()){String text=c.value(f);choices.get(f).setValue(text);fields.get(f).setText(text);}idleMode.getSelectionModel().select(c.idleMode());boostMode.getSelectionModel().select(c.boostMode());idleChannel.setValue(c.idleChannel());boostChannel.setValue(c.boostChannel());tpsChannel.setValue(c.tpsChannel());}
    AuxConfiguration candidate(){var values=new EnumMap<AuxConfiguration.Field,String>(AuxConfiguration.Field.class);fields.forEach((f,input)->values.put(f,input.getText()));return baseline.edited(values,idleMode.getSelectionModel().getSelectedIndex(),idleChannel.getValue(),boostMode.getSelectionModel().getSelectedIndex(),boostChannel.getValue(),tpsChannel.getValue());}
    void availability(DeviceIdentity d,boolean connected,boolean blocked,boolean engineArmed){if(!Objects.equals(identity,d)||!connected){identity=d;snapshot=null;}available=connected&&d!=null&&!blocked;armed=engineArmed;controls();}
    private void controls(){boolean edit=!busy&&!armed;read.setDisable(!available||busy);save.setDisable(!available||busy||armed||snapshot==null);base.setDisable(!edit);open.setDisable(!edit);backup.setDisable(busy);idleMode.setDisable(!edit);boostMode.setDisable(!edit);idleChannel.setDisable(!edit);boostChannel.setDisable(!edit);tpsChannel.setDisable(!edit);choices.values().forEach(c->c.setDisable(!edit));}
}
