EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr USLetter 11000 8500
encoding utf-8
Sheet 1 1
Title "AC Control Knob"
Date "2020-12-27"
Rev "1.00"
Comp "britishideas.com"
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Device:R_POT RV1
U 1 1 5FE921A4
P 4900 3500
F 0 "RV1" H 4831 3546 50  0000 R CNN
F 1 "Potentiometer" H 4831 3455 50  0000 R CNN
F 2 "" H 4900 3500 50  0001 C CNN
F 3 "~" H 4900 3500 50  0001 C CNN
	1    4900 3500
	1    0    0    -1  
$EndComp
$Comp
L Switch:SW_DPST_x2 SW1
U 1 1 5FE93410
P 3900 3500
F 0 "SW1" V 3946 3412 50  0000 R CNN
F 1 "Switch" V 3855 3412 50  0000 R CNN
F 2 "" H 3900 3500 50  0001 C CNN
F 3 "~" H 3900 3500 50  0001 C CNN
	1    3900 3500
	0    -1   -1   0   
$EndComp
$Comp
L Connector:Conn_01x04_Female J1
U 1 1 5FE93E95
P 5600 4050
F 0 "J1" H 5628 4026 50  0000 L CNN
F 1 "Knob Connector" H 5628 3935 50  0000 L CNN
F 2 "" H 5600 4050 50  0001 C CNN
F 3 "~" H 5600 4050 50  0001 C CNN
	1    5600 4050
	1    0    0    -1  
$EndComp
Text Notes 5350 3750 0    50   ~ 0
1- Green\n2 - Orange\n3 - Yellow\n4 - Red\n\n0.250 female spade connectors\n\nYellow-Orange must be low resistance when pot fully anti-clockwise\nand high resistance when pot fully clockwise
Wire Wire Line
	4900 3650 4900 4250
Wire Wire Line
	4900 4250 5400 4250
Wire Wire Line
	5400 4150 5200 4150
Wire Wire Line
	5200 4150 5200 3150
Wire Wire Line
	5200 3150 4900 3150
Wire Wire Line
	4900 3150 4900 3350
Wire Wire Line
	5050 3500 5050 4050
Wire Wire Line
	5050 4050 5400 4050
Wire Wire Line
	4900 3150 3900 3150
Wire Wire Line
	3900 3150 3900 3300
Connection ~ 4900 3150
Wire Wire Line
	5400 3950 3900 3950
Wire Wire Line
	3900 3950 3900 3700
$EndSCHEMATC
