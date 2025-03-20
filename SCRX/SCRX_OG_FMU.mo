within ;
model SCRX_OG_FMU
  OpenIPSL.Electrical.Controls.PSSE.ES.SCRX sCRX
    annotation (Placement(transformation(extent={{-46,-46},{46,46}})));
  Modelica.Blocks.Interfaces.RealInput EFD0(start=1.43511)
    annotation (Placement(transformation(extent={{-120,-100},{-80,-60}})));
  Modelica.Blocks.Interfaces.RealInput VOTHSG(start=0.0)
    annotation (Placement(transformation(extent={{-120,60},{-80,100}})));
  Modelica.Blocks.Interfaces.RealInput ECOMP(start=1.0)
    annotation (Placement(transformation(extent={{-120,-20},{-80,20}})));
  Modelica.Blocks.Interfaces.RealOutput EFD "Excitation Voltage [pu]"
    annotation (Placement(transformation(extent={{100,-10},{120,10}})));
  Modelica.Blocks.Interfaces.RealInput VUEL(start=1.43518) annotation (
      Placement(transformation(
        extent={{-20,-20},{20,20}},
        rotation=90,
        origin={-40,-100})));
  Modelica.Blocks.Interfaces.RealInput VOEL(start=0.0) annotation (Placement(
        transformation(
        extent={{-20,-20},{20,20}},
        rotation=90,
        origin={0,-100})));
  Modelica.Blocks.Interfaces.RealInput XADIFD(start=0.0) annotation (Placement(
        transformation(
        extent={{-20,-20},{20,20}},
        rotation=90,
        origin={40,-100})));
equation
  connect(sCRX.VOTHSG, VOTHSG) annotation (Line(points={{-50.6,18.4},{-74,18.4},
          {-74,80},{-100,80}}, color={0,0,127}));
  connect(sCRX.ECOMP, ECOMP)
    annotation (Line(points={{-50.6,0},{-100,0}}, color={0,0,127}));
  connect(sCRX.EFD0, EFD0) annotation (Line(points={{-50.6,-18.4},{-74,-18.4},{
          -74,-80},{-100,-80}}, color={0,0,127}));
  connect(sCRX.VUEL, VUEL) annotation (Line(points={{-18.4,-50.6},{-18.4,-76},{
          -40,-76},{-40,-100}}, color={0,0,127}));
  connect(sCRX.VOEL, VOEL)
    annotation (Line(points={{0,-50.6},{0,-100}}, color={0,0,127}));
  connect(sCRX.XADIFD, XADIFD) annotation (Line(points={{36.8,-50.6},{36.8,-76},
          {40,-76},{40,-100}}, color={0,0,127}));
  connect(sCRX.EFD, EFD)
    annotation (Line(points={{50.6,0},{110,0}}, color={0,0,127}));
  annotation (uses(OpenIPSL(version="3.1.0-dev"), Modelica(version="4.0.0")));
end SCRX_OG_FMU;
