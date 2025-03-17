# SQL Scripts for Time Tracker Project

This document describes the SQL scripts used to set up the database structure for the Time Tracker project and provides step-by-step instructions for configuring Supabase.

## Database Structure Overview

The project uses three main tables:

1. **rfid_events** - Records all RFID tag events (insertion/removal)
2. **tag_assignments** - Links RFID tags to projects and tasks
3. **device_assignments** - Tracks device information and locations

## SQL Scripts

### 1. rfid_events.sql

The primary table for storing all RFID tag events:

- Records tag insertions and removals with timestamps
- Stores tag IDs, types, and device IDs
- Includes WiFi and time sync status information

### 2. tag_assignments.sql

Links RFID tags to projects and tasks:

- Automatically adds new tags when first detected (via trigger)
- Allows for organizing tags by project and task

### 3. device_assignments.sql

Tracks information about physical devices:

- Stores friendly names, locations, and notes for each device
- Automatically adds new devices when first detected (via trigger)

## Supabase Setup Guide

### Step 1: Create a Supabase Project

1. Go to [Supabase](https://supabase.com/) and sign in
2. Create a new project with a name like "Time-Tracker"
3. Note your project URL and API keys (you'll need these later)

### Step 2: Set Up Database Tables

Execute the SQL scripts in the following order:

1. First, run `rfid_events.sql` to create the main events table
   - Validation: Check that the table appears in the Supabase Table Editor
   - Validation: Verify indexes are created (check "Indexes" in Table Editor)

2. Next, run `tag_assignments.sql` to create the tag assignments table and trigger
   - Validation: Check that the table appears in the Table Editor
   - Validation: Verify the trigger is created (check "Triggers" in Database section)

3. Finally, run `device_assignments.sql` to create the device assignments table and trigger
   - Validation: Check that the table appears in the Table Editor
   - Validation: Verify the trigger is created

### Step 3: Configure n8n Integration

1. Get Supabase Service Role Key:
   - Go to your Supabase project dashboard
   - Navigate to Project Settings -> API
   - Copy the `service_role` key (starts with `eyJ...`)
   - This key bypasses RLS and has full access to the database

2. Configure n8n Workflow:

   ```txt
   Webhook Node -> Supabase Node
   ```

3. Supabase Node Configuration:
   - Add new credentials
     - Host: Your Supabase project URL (e.g., `https://xxxxxxxxxxxx.supabase.co`)
     - Service Role Key: Paste the key from step 1
     - Name these credentials (e.g., "RFID-Supabase")
   - Operation: Insert
   - Table: rfid_events
   - Data Mapping (from webhook payload):

     ```txt
     {
       "timestamp": "{{$json.rfid_poll_result.timestamp}}",
       "event_type": "{{$json.rfid_poll_result.event_type}}",
       "tag_present": "{{$json.rfid_poll_result.tag_present}}",
       "tag_id": "{{$json.rfid_poll_result.tag_id}}",
       "tag_type": "{{$json.rfid_poll_result.tag_type}}",
       "wifi_status": "{{$json.rfid_poll_result.wifi_status}}",
       "time_status": "{{$json.rfid_poll_result.time_status}}",
       "device_id": "{{$json.rfid_poll_result.device_id}}"
     }
     ```

### Step 4: Test the Integration

1. Deploy the ESP32 firmware with the webhook URL pointing to your n8n instance
2. Scan an RFID tag with the device
3. Verify data flow:
   - Check that events appear in the `rfid_events` table
   - Verify that new tags are automatically added to `tag_assignments`
   - Verify that new devices are automatically added to `device_assignments`

## Database Design Notes

- **Row Level Security (RLS)**: All tables have RLS enabled with policies that allow authenticated users full access
- **Automatic Timestamps**: The `rfid_events` table has automatic handling for `created_at` and `updated_at`
- **Triggers**: Both `tag_assignments` and `device_assignments` have triggers that automatically add new entries
- **Indexes**: All tables have indexes on key columns for efficient querying
- **Data Types**: Uses PostgreSQL-specific types like `TIMESTAMPTZ` for proper timezone handling

## Maintenance and Troubleshooting

### Modifying Triggers

If you need to modify a trigger function:

  1. First, drop the existing trigger:

    ```sql
    DROP TRIGGER trigger_name ON table_name;
    ```

  2. Then, update the function and recreate the trigger:

  ```sql
  CREATE OR REPLACE FUNCTION function_name() ... 
  CREATE TRIGGER trigger_name ...
  ```

### Backing Up Data

To back up your data from Supabase:

1. Go to Project Settings -> Database
2. Click "Database Backups"
3. Download the latest backup or create a new one
