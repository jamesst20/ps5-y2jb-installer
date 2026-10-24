<script>
	import { Dialog as DialogPrimitive } from "bits-ui";

	let {
		ref = $bindable(null),
		type = "button",
		...restProps
	} = $props();
</script>

<DialogPrimitive.Close bind:ref data-slot="dialog-close" {type} {...restProps} />