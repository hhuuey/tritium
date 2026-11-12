import dagster as dg

case_partitions = dg.DynamicPartitionsDefinition(name="cases")
