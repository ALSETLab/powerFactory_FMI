within ;
model scrx_rebuild

  parameter Real T_AT_B=0.1 "Ratio between regulator numerator (lead) and denominator (lag) time constants";
  parameter OpenIPSL.Types.Time T_B=1 "Regulator denominator (lag) time constant";
  parameter OpenIPSL.Types.PerUnit K=100 "Excitation power source output gain";
  parameter OpenIPSL.Types.Time T_E=0.005 "Excitation power source output time constant";
  parameter OpenIPSL.Types.PerUnit E_MIN=-10 "Minimum exciter output";
  parameter OpenIPSL.Types.PerUnit E_MAX=10 "Maximum exciter output";
  parameter Boolean C_SWITCH=false "Feeding selection. False for bus fed, and True for solid fed";
  parameter Real r_cr_fd=10 "Ratio between crowbar circuit resistance and field circuit resistance";
  Modelica.Blocks.Interfaces.RealOutput EFD "Excitation Voltage [pu]"
    annotation (Placement(transformation(extent={{100,-10},{120,10}}), iconTransformation(extent={{100,-10},
            {120,10}})));
  Modelica.Blocks.Interfaces.RealInput ECOMP annotation (Placement(
        transformation(
        extent={{-20,-20},{20,20}},
        origin={-100,0}), iconTransformation(extent={{-10,-10},{10,10}}, origin={-110,0})));
  Modelica.Blocks.Sources.Constant VoltageReference(k=1)
    annotation (Placement(transformation(extent={{-94,30},{-74,50}})));
  Modelica.Blocks.Math.Add DiffV(k2=-1)
    annotation (Placement(transformation(extent={{-60,-10},{-40,10}})));
  OpenIPSL.NonElectrical.Continuous.LeadLag leadLag(
    K=1,
    T1=T_AT_B*T_B,
    T2=T_B,
    y_start=1.5/K)
    annotation (Placement(transformation(extent={{-20,-10},{0,10}})));
  OpenIPSL.NonElectrical.Continuous.SimpleLagLim simpleLagLim(
    K=K,
    T=T_E,
    y_start=1.5,
    outMax=E_MAX,
    outMin=E_MIN)
    annotation (Placement(transformation(extent={{18,-10},{38,10}})));
  Modelica.Blocks.Math.Product product1
    annotation (Placement(transformation(extent={{68,-10},{88,10}})));
equation
  connect(ECOMP, DiffV.u2) annotation (Line(points={{-100,0},{-82,0},{-82,-6},{
          -62,-6}}, color={0,0,127}));
  connect(VoltageReference.y, DiffV.u1)
    annotation (Line(points={{-73,40},{-62,40},{-62,6}}, color={0,0,127}));
  connect(leadLag.y, simpleLagLim.u)
    annotation (Line(points={{1,0},{16,0}}, color={0,0,127}));
  connect(DiffV.y, leadLag.u)
    annotation (Line(points={{-39,0},{-22,0}}, color={0,0,127}));
  connect(simpleLagLim.y, product1.u2)
    annotation (Line(points={{39,0},{56,0},{56,-6},{66,-6}}, color={0,0,127}));
  connect(product1.y, EFD)
    annotation (Line(points={{89,0},{110,0}}, color={0,0,127}));
  connect(product1.u1, ECOMP) annotation (Line(points={{66,6},{66,22},{-88,22},
          {-88,0},{-100,0}}, color={0,0,127}));
  annotation (
    Icon(coordinateSystem(preserveAspectRatio=false)),
    Diagram(coordinateSystem(preserveAspectRatio=false)),
    uses(Modelica(version="4.0.0"), OpenIPSL(version="3.1.0-dev")));
end scrx_rebuild;
