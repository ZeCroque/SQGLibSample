Scriptname SQGTestActivator extends ObjectReference  
 
import SQGLib

Message Property SQGTestMessage Auto
ActorBase Property SQGTestTargetNPC Auto

Event OnActivate(ObjectReference akActionRef)
	Int result = SQGTestMessage.Show()
	If (result == 0) 
		ObjectReference target = Game.GetPlayer().PlaceAtMe(SQGTestTargetNPC, 1, true)
		Debug.MessageBox(GenerateQuest(target))
	ElseIf (result == 1) 
		PrintGeneratedQuests()
	Else
		DraftDebugFunction()
	EndIf 
endEvent


