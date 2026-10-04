*** Settings ***
Library   String
Library   SerialLibrary

*** Variables ***
${com}   	COM11
${baud} 	115200
${board}	nRF
${rseq}      000100X
${wseq}      0000010X
${correct}   60X
${error}     -1X

*** Test Cases ***
Connect Serial
	Log To Console  Connecting to ${board}
	Add Port  ${com}  baudrate=${baud}  encoding=ascii
	Port Should Be Open  ${com}
	Reset Input Buffer
	Reset Output Buffer

Send The Correct Sequence

	Write Data   ${rseq}   encoding=ascii 
	Log To Console   Send sequence ${rseq}

	${read} =   Read Until   terminator=58   encoding=ascii 

	Log To Console   Received ${read}
	
	Should Be Equal As Strings   ${read}    ${correct}
	Log To Console    Tested ${read} is same as ${correct}

Send The Wrong Sequence
    Sleep  1s
    Reset Input Buffer
    Reset Output Buffer
    Write Data   ${wseq}   encoding=ascii
    Log To Console   Send sequence ${wseq}

    ${read} =   Read Until   terminator=58   encoding=ascii

    Log To Console   Received ${read}
    Should Be Equal As Strings   ${read}    ${error}
    Log To Console    Tested ${read} is same as ${error}


	
Disconnect Serial
	Log To Console  Disconnecting ${board}
	[TearDown]  Delete Port  ${com}


	
	
	

